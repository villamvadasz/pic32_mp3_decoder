@echo off
echo const char RANDOM_VALUE[16]  __attribute__ ((address(0x9D008160))) = {>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
SET /A test=%RANDOM% %%% 256 >>.\src\randomValue.c
echo 	%test%,              >>.\src\randomValue.c
echo };                      >>.\src\randomValue.c

