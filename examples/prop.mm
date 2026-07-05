$c wff |- -. -> <-> \/ /\ ( ) $.
$v ph ps ch $.

wph $f wff ph $.
wps $f wff ps $.
wch $f wff ch $.
wnot $a wff -. ph $.
wim $a wff ( ph -> ps ) $.
wbi $a wff ( ph <-> ps ) $.
wor $a wff ( ph \/ ps ) $.
wand $a wff ( ph /\ ps ) $.

$( Axiom Simp $)
ax-simp $a |- ( ph -> ( ps -> ph ) ) $.

$( Axiom Frege $)
ax-frege $a |- ( ( ph -> ( ps -> ch ) ) -> ( ( ph -> ps ) -> ( ph -> ch ) ) ) $.

$( Axiom Transp $)
ax-transp $a |- ( ( -. ph -> -. ps ) -> ( ps -> ph ) ) $.

$( Rule of Modus Ponens $)
${
	min $e |- ph $.
	maj $e |- ( ph -> ps ) $.
	ax-mp $a |- ps $.
$}


id $p |- ( ph -> ph ) $=
	wph wph wph wim wim
	wph wph wim
	wph wph ax-simp
	wph wph wph wim wph wim wim
	wph wph wph wim wim wph wph wim wim
	wph wph wph wim ax-simp
	wph wph wph wim wph ax-frege
	ax-mp
	ax-mp
$.
