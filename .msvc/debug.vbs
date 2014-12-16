' Visual Studio QEMU debugging script.
'
' I like invoking vbs as much as anyone else, but we need to download and unzip a
' bunch of files, as well as launch QEMU, and neither Powershell or a standard batch
' can do that without having an extra console appearing.
'
' Note: You may get a prompt from the Windows firewall when trying to download files

' Modify these variables as needed
QEMU_PATH  = "C:\Program Files\qemu\"
QEMU_EXE   = "qemu-system-x86_64w.exe"
OVMF_ZIP   = "OVMF-X64-r15214.zip"
OVMF_BIOS  = "OVMF.fd"
FTP_SERVER = "ftp.heanet.ie"
FTP_FILE   = "pub/download.sourceforge.net/pub/sourceforge/e/ed/edk2/OVMF/" & OVMF_ZIP
FTP_URL    = "ftp://" & FTP_SERVER & "/" & FTP_FILE
CONF       = WScript.Arguments(0)
FS         = WScript.Arguments(1)
BIN        = WScript.Arguments(2)
LOG_LEVEL  = 0
If (CONF = "Debug") Then
  LOG_LEVEL = 4
End If
IMG_EXT    = ".img"
If ((FS = "iso9660") Or (FS = "udf")) Then
  IMG_EXT  = ".iso"
ElseIf ((FS = "ntfs") Or (FS = "exfat")) Then
  IMG_EXT  = ".vhd"
End If
IMG        = FS & IMG_EXT
IMG_ZIP    = FS & ".zip"
IMG_URL    = "http://efi.akeo.ie/test/" & IMG_ZIP
DRV        = FS & "_x64.efi"
DRV_URL    = "http://efi.akeo.ie/downloads/efifs-0.6.1/x64/" & DRV
MNT        = "fs1:"
If ((FS = "bfs") Or (FS = "btrfs") Or (FS = "hfs") Or (FS = "jfs") Or (FS = "xfs")) Then
  MNT      = "fs3:"
ElseIf (FS = "zfs") Then
  ' No idea why we get an '@' directory on ZFS, but it's "working" if we proceed from there
  MNT      =  MNT & "/@"
End If


' Globals
Set fso = CreateObject("Scripting.FileSystemObject") 
Set shell = CreateObject("WScript.Shell")

' Download a file from FTP
Sub DownloadFtp(Server, Path)
  Set file = fso.CreateTextFile("ftp.txt", True)
  Call file.Write("open " & Server & vbCrLf &_
    "anonymous" & vbCrLf & "user" & vbCrLf & "bin" & vbCrLf &_
    "get " & Path & vbCrLf & "bye" & vbCrLf)
  Call file.Close()
  Call shell.Run("%comspec% /c ftp -s:ftp.txt > NUL", 0, True)
  Call fso.DeleteFile("ftp.txt")
End Sub

' Download a file from HTTP
Sub DownloadHttp(Url, File)
  Const BINARY = 1
  Const OVERWRITE = 2
  Set xHttp = createobject("Microsoft.XMLHTTP")
  Set bStrm = createobject("Adodb.Stream")
  Call xHttp.Open("GET", Url, False)
  Call xHttp.Send()
  With bStrm
    .type = BINARY
    .open
    .write xHttp.responseBody
    .savetofile File, OVERWRITE
  End With
End Sub

' Unzip a specific file from an archive
Sub Unzip(Archive, File)
  Const NOCONFIRMATION = &H10&
  Const NOERRORUI = &H400&
  Const SIMPLEPROGRESS = &H100&
  unzipFlags = NOCONFIRMATION + NOERRORUI + SIMPLEPROGRESS
  Set objShell = CreateObject("Shell.Application")
  Set objSource = objShell.NameSpace(fso.GetAbsolutePathName(Archive)).Items()
  Set objTarget = objShell.NameSpace(fso.GetAbsolutePathName("."))
  ' Only extract the file we are interested in
  For i = 0 To objSource.Count - 1
    If objSource.Item(i).Name = File Then
      Call objTarget.CopyHere(objSource.Item(i), unzipFlags)
    End If
  Next
End Sub


' Check that QEMU is available
If Not fso.FileExists(QEMU_PATH & QEMU_EXE) Then
  Call WScript.Echo("'" & QEMU_PATH & QEMU_EXE & "' was not found." & vbCrLf &_
    "Please make sure QEMU is installed or edit the path in '.msvc\debug.vbs'.")
  Call WScript.Quit(1)
End If

' Fetch the Tianocore UEFI BIOS and unzip it
If Not fso.FileExists(OVMF_BIOS) Then
  Call WScript.Echo("The latest OVMF BIOS file, needed for QEMU/EFI, " &_
   "will be downloaded from: " & FTP_URL & vbCrLf & vbCrLf &_
   "Note: Unless you delete the file, this should only happen once.")
  Call DownloadFtp(FTP_SERVER, FTP_FILE)
  Call Unzip(OVMF_ZIP, OVMF_BIOS)
  Call fso.DeleteFile(OVMF_ZIP)
End If
If Not fso.FileExists(OVMF_BIOS) Then
  Call WScript.Echo("There was a problem downloading or unzipping the OVMF BIOS file.")
  Call WScript.Quit(1)
End If

' Fetch the VHD image
If Not fso.FileExists(IMG) Then
  Call DownloadHttp(IMG_URL, IMG_ZIP)
  Call Unzip(IMG_ZIP, IMG)
  Call fso.DeleteFile(IMG_ZIP)
End If
If Not fso.FileExists(IMG) Then
  Call WScript.Echo("There was a problem downloading or unzipping the " & FS & " image.")
  Call WScript.Quit(1)
End If

' Copy the files where required, and start QEMU
Call shell.Run("%COMSPEC% /c mkdir ""image\efi\boot""", 0, True)
Call fso.CopyFile(BIN, "image\" & DRV, True)
' Create a startup.nsh that: sets logging, loads the driver and executes an "Hello World" app from the disk
Set file = fso.CreateTextFile("image\efi\boot\startup.nsh", True)
Call file.Write("set FS_LOGGING " & LOG_LEVEL & vbCrLf &_
  "load fs0:/" & DRV & vbCrLf &_
  "map -r" & vbCrLf &_
  MNT & "/EFI/Boot/bootx64.efi" & vbCrLf)
Call file.Close()
' Add something like "-S -gdb tcp:127.0.0.1:1234" if you want to use gdb to debug
Call shell.Run("""" & QEMU_PATH & QEMU_EXE & """ -L . -bios OVMF.fd -net none -hda fat:image -hdb " & IMG, 1, True)
