system("mkdir -p build");
system("rsync -av . build/src --exclude build --exclude artifacts --exclude .git --delete") eq 0 or die("failed rsync");
system("cd build && zip -r ../builds.zip *") eq 0 or die("failed creating builds.zip");
