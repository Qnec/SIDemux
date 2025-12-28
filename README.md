# SIDemux

SIDemux (Standard Input Demux) takes in a list of bash commands as cli arguments and a stream of inputs on stdin, then depending on the selector at the beginning of the input SIDemux sends the input without the selector number to stdin of the selected bash command.

SIDemux has the switches s,i,u,d
\-d [character] specifies the selector separator, which is the character in between the selector number and the input contents by default it is `,`
\-i [character] specifies the delimiter character, which is the character inputs end with, by default it is the newline character
\-u [number] specifies the number of microseconds to wait after forking processes to wait before starting to send input to them
\-d [number] specifies the number of microseconds to wait in between inputs before sending another input to its selected process

with the call `sidemux 'paste - <(yes 0)' 'paste - <(yes 1)' 'paste - <(yes 2)' 'paste - <(yes3)'`
and the input
```
0,example line 1
1,example line 2
2,example line 3 with special characters ,1234567890
3,example line 4
```
one would see the output
```
example line 1  0
example line 2  1
example line 3 with special characters ,1234567890  2
example line 4  3
```

with the call `sidemux 'cat > output0.txt' 'cat output1.txt'`
and the input
```
0,line1
1,line2
0,line3
0,line4
1,line5
1,line6
```
output0.txt would end up with the contents
```
line1
line3
line4
line
```
and output1.txt would end up with the contents
```
line2
line5
line6
```
