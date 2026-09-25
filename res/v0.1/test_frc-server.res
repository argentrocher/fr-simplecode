FRC-SERVER[auto];
"0.1";
"0x1";
"0x404";
publiched{
"argentropcher";
};
ref{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/frc-server/res/v0.1/test_frc-server.res";
"test_frc-server.res";
"0.1";
20260118;
};
option{
0xFF;
1;
1;
".FRC";
0;
"FR";
0;
0;
0;
0;
0xFF;
0xFF;
};
pilot-require{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/libwinpthread-1.dll";
"libwinpthread-1.dll";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/fr-simplecode_instal_frc.ps1";
"fr-simplecode_instal_frc.ps1";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/file_built_by_fr-simplecode/fr-simplecode.conf";
"fr-simplecode.conf";
}
pilot{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/fr-simplecode.exe";
"fr-simplecode.exe";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/fr-simplecode0.2.exe";
"fr-simplecode0.2.exe";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/fr-simplecode0.3.exe";
"fr-simplecode0.3.exe";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/fr-simplecode0.4.exe";
"fr-simplecode0.4.exe";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/fr-simplecode0.4.1.exe";
"fr-simplecode0.4.1.exe";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/fr-simplecode0.5.exe";
"fr-simplecode0.5.exe";
};
pilot-commandline{
"fr-simplecode0.4.exe"{
"--start:<PATH>";
1;
"start file";
};
"fr-simplecode0.4.1.exe"{
"--start:<PATH>";
1;
"start file";
"--arg:<ARG>";
2;
"arg app";
"--en";
"101";
"server language";
"--fr";
"100";
"server language";
};
"fr-simplecode0.5.exe"{
"--start:<PATH>";
1;
"start file";
"--arg:<ARG>";
2;
"arg app";
"--en";
"101";
"server language";
"--fr";
"100";
"server language";
};
};
pilot-lib{
"fr-simplecode0.3.exe"{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/math%2Btimelibrairy.frc";
"math+timelibrairy.frc";
"lib_frc/lib/";
"librairy of maths and time";
};
"fr-simplecode0.4.exe"{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/math%2Btimelibrairy.frc";
"math+timelibrairy.frc";
"lib_frc/lib/";
"librairy of maths and time";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/frc_os.c";
"frc_os.c";
"lib_frc/lib/frc utilitaire";
"Code C for os.dll / os.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/frc_os.dll";
"frc_os.dll";
"lib_frc/lib/frc utilitaire";
"DLL for os.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/os.frc";
"os.frc";
"lib_frc/lib/frc utilitaire";
"Librairy to read/write files";
};
"fr-simplecode0.4.1.exe"{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/math%2Btimelibrairy.frc";
"math+timelibrairy.frc";
"lib_frc/lib/";
"librairy of maths and time";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/frc_os.c";
"frc_os.c";
"lib_frc/lib/frc utilitaire";
"Code C for os.dll / os.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/frc_os.dll";
"frc_os.dll";
"lib_frc/lib/frc utilitaire";
"DLL for os.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/os.frc";
"os.frc";
"lib_frc/lib/frc utilitaire";
"Librairy to read/write files";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/cmd_color.param";
"cmd_color.param";
"lib_frc/lib/frc utilitaire";
"memory for cmd_color.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/cmd_color.frc";
"cmd_color.frc";
"lib_frc/lib/frc utilitaire";
"Librairy to write color in cmd";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/frc_service.dll";
"frc_service.dll";
"lib_frc/lib/frc utilitaire";
"DLL for service.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/service.frc";
"service.frc";
"lib_frc/lib/frc utilitaire";
"Librairy to call all function in dll up to 10 arguments";
};
"fr-simplecode0.5.exe"{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/math%2Btimelibrairy.frc";
"math+timelibrairy.frc";
"lib_frc/lib/";
"librairy of maths and time";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/frc_os.c";
"frc_os.c";
"lib_frc/lib/frc utilitaire";
"Code C for os.dll / os.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/frc_os.dll";
"frc_os.dll";
"lib_frc/lib/frc utilitaire";
"DLL for os.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/os.frc";
"os.frc";
"lib_frc/lib/frc utilitaire";
"Librairy to read/write files";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/cmd_color.param";
"cmd_color.param";
"lib_frc/lib/frc utilitaire";
"memory for cmd_color.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/cmd_color.frc";
"cmd_color.frc";
"lib_frc/lib/frc utilitaire";
"Librairy to write color in cmd";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/frc_service.dll";
"frc_service.dll";
"lib_frc/lib/frc utilitaire";
"DLL for service.frc";
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/exemple_script_0.4/frc_utilitaire/service.frc";
"service.frc";
"lib_frc/lib/frc utilitaire";
"Librairy to call all function in dll up to 10 arguments";
};
};
server-dll{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/dll_frc/http_serveur_dll.dll";
"http_serveur_dll.dll";
0;
1;
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/dll_frc/http_serveur_dll2.dll";
"http_serveur_dll2.dll";
0;
2;
};
frc-server-res{
"<MAIN>"{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/frc-server/main/v0.1/test.html";
"main.html";
0;
};
"fr-simplecode0.4.exe"{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/frc-server/main/v0.1/frc-server_example.frc";
"frc-server_example.frc";
0x0102;
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/frc-server/main/v0.1/server_frc_test.html";
"server_frc_test.html";
0x0200;
};
"fr-simplecode0.4.1.exe"{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/frc-server/main/v0.1/frc-server_example.frc";
"frc-server_example.frc";
0x0102;
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/frc-server/main/v0.1/server_frc_test.html";
"server_frc_test.html";
0x0200;
};
"fr-simplecode0.5.exe"{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/frc-server/main/v0.1/frc-server_example.frc";
"frc-server_example.frc";
0x0102;
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/frc-server/main/v0.1/server_frc_test.html";
"server_frc_test.html";
0x0201;
};
};
optional-res{
"https://raw.githubusercontent.com/argentrocher/fr-simplecode/refs/heads/main/frc.png";
"frc.png";
"";
};




