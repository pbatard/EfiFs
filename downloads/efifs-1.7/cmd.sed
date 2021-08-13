# Use: sed -E -b -i -f ../cmd.sed index.html
s/ alt="\[   \]"//g
s/a href="([^"]*\.efi)/a href="https:\/\/github.com\/pbatard\/efifs\/releases\/download\/v1.7\/\1/g

