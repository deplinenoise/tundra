system("rsync -av .. src --exclude build --exclude artifacts --exclude .git --delete") eq 0 or die("failed creating builds.zip");
system("zip -r builds.zip *") eq 0 or die("failed creating builds.zip");
