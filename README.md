<img src="img/11mm.png" width=100 height=auto />

*11mm* is a (soon-to-be, not yet, WIP) proof verifier for a less rigid dialect of the [Metamath](https://us.metamath.org) language, written in C++11. Differences from the main language include:
 - Comments can now nest (e. g. `$( $( hi mom $) $)`);
 - You may start comments/statements inside an includer file and end them in an included file, etc. (the preprocessor acts like an actual preprocessor now);
 - You can now shadow `$c` and `$v` with each other, or with themselves inside another block;
 - You can also shadow `$e` and `$f` in a similar manner.
