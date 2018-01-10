system("mkdir -p build");
system("rsync -av . build/src --exclude build --exclude artifacts --exclude .git --delete") eq 0 or die("failed rsync");
system("zip -r builds.zip build/*") eq 0 or die("failed creating builds.zip");
