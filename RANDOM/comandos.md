### Cable.c
Depois de executar, tenho de usar 2 portas seriais virtuais
1.  /dev/ttyS10 
2.  /dev/ttyS11
3.  

`socat -d -d pty,raw,echo=0 pty,raw,echo=0`