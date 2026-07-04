$c 0 suc ( ) = wff nat |- $.

$v x y z $.
natx $f nat x $.
naty $f nat y $.
natz $f nat z $.
weq $a wff x = y $.

ax-nzero $a nat 0 $.
ax-eqrefl $a |- x = x $.
${
	ax-eqsym.e1 $e |- x = y $.
	ax-eqsym $a |- y = x $.
$}
${
	ax-eqtr.e1 $e |- x = y $.
	ax-eqtr.e2 $e |- y = z $.
	ax-eqtr $a |- x = z $.
$}
ax-succlo $a nat ( suc x ) $.
${
	ax-sucinj.e1 $e |- ( suc x ) = ( suc y ) $.
	ax-sucinj $a |- x = y $.
$}
