del /q .yam\*
rmdir .yam
del /q project\linkedobjects.txt
del /q project\cyclic*.txt
del /q project\fooobjects*.txt
del /q submodules\*.obj
del /q submodules\*objects*.txt
del /q submodules\group_*.txt
del /q submodules\sm1\*.obj
del /q submodules\sm1\*objects*.txt
del /q submodules\sm1\buildfile_yam_gen.txt
rmdir /s /q submodules\sm1\nep
rmdir /s /q submodules\sm1\generated
del /q submodules\sm2\*.obj
del /q submodules\sm3\*.obj
rmdir /s /q demo\compileAndLink\bin
del /q demo\groups\app\A\*.obj
del /q demo\groups\app\B\*.obj
del /q demo\groups\app\*.exe
del /q demo\groups\platform\*.obj
rmdir /s /q demo\ignoreOutputs\nep
rmdir /s /q demo\imageProcessing\generated