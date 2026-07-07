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
	ax-mp.1 $e |- ph $.
	ax-mp.2 $e |- ( ph -> ps ) $.
	ax-mp $a |- ps $.
$}


$( Identity $)
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

$( Syllogism $)
${
	syl.1 $e |- ( ph -> ps ) $.
	syl.2 $e |- ( ps -> ch ) $.
	syl $p |- ( ph -> ch ) $=
		wph wps wim
		wph wch wim
		syl.1
		wph wps wch wim wim
		wph wps wim wph wch wim wim
		wps wch wim
		wph wps wch wim wim
		syl.2
		wps wch wim wph ax-simp
		ax-mp
		wph wps wch ax-frege
		ax-mp
		ax-mp
	$.
$}

$( Hypothesis Introduction $)
${
	a1i.1 $e |- ph $.
	a1i.p $p |- ( ps -> ph ) $=
		wph
		wps wph wim
		a1i.1
		wph wps ax-simp
		ax-mp
	$.
$}
