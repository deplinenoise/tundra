use File::Path qw(mkpath);

mkpath("artifacts/$^O");

if ($^O eq "MSWin32")
{
    system("msbuild vs2012\\Tundra.sln /P:Configuration=Release") eq 0 or die("failed msbuild");
    system("copy vs2012\\x64\\Release\\tundra2.exe artifacts\\$^O") eq 0 or die("failed copy");
} else
{
    system("make") eq 0 or die("failed make");
    system("cp build/tundra2 artifacts/$^O/") eq 0 or die("failed copy");
}


