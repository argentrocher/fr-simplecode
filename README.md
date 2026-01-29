<head><meta name="google-site-verification" content="k1Ku-V2S2bvRYoVdqf_-UaNnDOuzXFqMYL2xyv3j_L4" /></head>

fr-simplecode est un language basique de codage pour windows (64 bits uniquement), découvrer le ! 

<br>INFO : <strong>frc_notepad.xml</strong> permet d'avoir un bon rendu du code (hormis pour les chaînes entre " car l'imbrication des fonctions internes text() et num() qui posèdent également des " internes ne peuvent pas être ignorés.) <br>

dernière version : fr-simplecode0.4.1 (avec pile de script sur fr-simplecode0.4.1 et pas sur fr-simplecode0.4-> n'est plus mi à jour car sans pile, c'est moins performant) (! récent, beaucoup de modification par rapport à la 0.3 mais en développement, le fichier change régulièrement et peu planter pour les nouvelles fonctions (actuelment 0.4.1 est stable mais des améliorations sont encore à venir sous se même nom) (actuellement 19913 lignes de code C)
<br>nouveauté :<br>
les nombres on désormais des virgules jusqu'à 14 de manière stable (! sans entier), un mode normal et scientifique pour l'affichage des nombres<br>
<code>pow(v1;v2)</code> permet de faire des nombres avec des puissances<br>
<code>hash("arg")</code> permet de haché un texte en un nombre constant en fonction du texte (fonction irréversible, avec le hash, impossible à reconstruire)<br>
<code>block_executed()</code> renvoie le nombre de block executé par le programme depuis le début du script<br>
<code>include(arg1)(arg2)</code> permet d'inclure des scripts, arg1 est le chemin du fichier (il peut également être un lien en http/https (tester sur github, https ne prend que du tls1.2)), arg2 est le nom attribué : ex: <code>[[[{include(https://argentrocher.github.io/fr-simplecode/exemple_script_0.4/func_frc_0.4.frc)(abc)}{abc.boucle(10)}]]]</code><br>
pour gérer le http/https, fr-simplecode créer les fichiers récupérés sur internet de stockage sur <code>lib_frc/web_include/</code>, une table de registre est également créer sur <code>lib_frc/register_web_frc.txt</code> qui comptient des validités (si l'on dépasse 1h sans téléchargé, il recharge le code depuis internet, sinon il garde le même fichier), si vous voulez recharger tout les liens, supprimé le fichier. (! ne pas écrire de fichier <code>register_web_frc_tmp.txt</code> car fr-simplecode fait des copies sur cette emplacement et vérifie les validité du registre)
<code>sound_generator(arg1;arg2;arg3;arg4;arg5)</code> permet de générer des sons<br><br>
les signes d'opération dans les calculs on été agrandi : <ul><li><code>%</code> pour le modulo (! modulo sur entier uniquement, pour un modulo à virgule, utilisez <code>modulo(val1;val2)</code>)</li>
<li><code>#</code> permet de faire des divisions entières</li>
<li><code>^</code> permet de faire des calculs de puissance (opération à la priorité le plus élevé) comme pow(val1;val2)</li>
</ul>
<code>factorial(val)</code> permet de renvoyé le nombre val en factoriel<br>
<code>[/n]</code>,<code>[/0]</code>,<code>[/l0]</code>,<code>[/l1]</code>,<code>[/l2]</code>,<code>[/l3]</code>,<code>[/l4]</code>,<code>[/l5]</code>,<code>[/l6]</code> sont désormais disponibles partout dans les parseurs de texte, sortie (print,speak) et dll.<br>
<code>/</code> permet désormais de les désactivés, leurs duplications collés <code>//</code> renvoie <code>/</code> (parsing standard)<br>
<code>box_error(parsing)</code>/<code>boite_erreur(parsing)</code> fonctionne comme un <code>print()</code> mais avec une boite d'erreur windows<br>
<code>?func.</code> permet de définir des fonctions (uniquement dans des fichiers lancé directement ou avec <code>use_script()</code>), le programme recherche automatiquement le nom de la fonction dans le fichier si dans text() ou num() et l'exécute, avec possibilité de <code>return()</code> ou <code>retourne()</code> avec le même parsing que des <code>print()</code><br>
il est également possible de passé des arguments variables ou préfixé ex: <code>?func.abc(var_local|a|;var_local|b|=10)[[[{print(var_local|a|" "var_local|b|)}]]]</code> si on l'appel avec : <code>[[[{abc(1; )}]]]</code> affichera <code>1 10</code>, si on l'appel avec : <code>[[[{abc(3;var_local|b|=11)}]]]</code> affichera <code>3 11</code><br>
<code>var_local|name|=--</code> et <code>var|name|=--</code> peuvent désormais supprimé le dernier charactère si la variable est en texte (! pour <code>var|name|=""</code> considérer comme 0 donc numérique mais pas pour var_local),<br>si on utlise <code>var_local|name|=++</code> et <code>var|name|=++</code> sur des variables textuels, cela ajoute un espace à la fin<br>
<code>num()</code> peut désormais prendre un deuxième argument qui limite le nombre de chiffres après la virgule : <code>num(3/2+1;0)</code> affiche <code>2</code>, <code>num(3/2+1;2)</code> affiche <code>1.67</code><br>
les tables sur 0.4.1 (0.4 n'a pas de modification par rapport à la 0.3 pour cela car plus mis à jour, télécharger la 0.4.1) :<br>
<code>table||=++</code> ajoute une case de valeur <code>0.00</code><br>
<code>table||=--</code> retire la dernière case finale (peut former une table vide <code>[[]]</code> pour pouvoir la remplir, <code>table||.edit()()</code> ne marchera pas, utiliser =++ ou .add())<br>
<code>table||.become("arg")</code> avec arg une table  qui peut être fournit avec <code>text()</code> (permet de lire une table de la 0.3), cela ajoute les guillemet " " autour des valeurs textuelle automatiquement (! pas d'opération de table si dessous)<br>
<code>table||=...</code> peut prendre plusieurs tables ensembles et faire des calculs, pour reprendre une table, il faut écrire juste <code>table||</code>,<br>pour ajouté des cases, utilisé + : soit une nouvelle table soit à la main (exemple: <code>table|a|=table|a|+[[1;2]]</code> ajoutera les deux cases 1;2 à la fin de la table)<br>
pour retiré des cases, utilisé - : les cases de la table après le - dans une table|| ou dans [[...]] pour chacune des cases, pour chaque case, le programme regarde si les cases existe en partant de la fin (! pas de niveau de profondeur, seul ne niveau de base est touché), si oui il les retire (exemple : avec <code>table|a|=[[3;56;9;67;8;56;77;0;56;[[56;9]];18]]</code>; <code>table|a|=table|a|-[[56;56]]</code>, table|a|--><code>[[3.00;56.00;9.00;67.00;8.00;77.00;0.00;[[56.00;9.00]];18.00]]</code> car elle a retiré les deux dernier 56 en partant de la fin sans compter les [[ ]] interne)<br>
les opérateurs + - * / % (%=modulo)  # (#=division entière) et ^ (^=puissance) peuvent également faire des calculs au niveau 0 (pas de sous table) et uniquement à la fin (si un de ses opérateurs est rencontrer, toute la fin est considérer comme un nombre, donc plus de [[ ]] après, et il est conseillé de ne pas mélanger d'opération de table avec le signe - et de calcul en même temps car les - de table ne seront pas compris), exemple avec <code>table|a|=[[3;56;9;67;8;56;77;0;56;[[56;9]];18]]</code>: <code>table|a|=table|a|+3</code>, table|a|--><code>[[6.00;59.00;12.00;70.00;11.00;59.00;80.00;3.00;59.00;[[56.00;9.00]];21.00]]</code> car touche tout sauf les sous tables (et le texte aussi qui reste texte si entre " ")<br><br>

<code>table||</code> est désormais totalement disponible en <code>table_local||</code> (seul la fonction <code>table_local||=join(;)</code> n'exsite pas (car cette fonction est hors d'usage et ne sera plus du tout disponible même en global))<br>
 
<p><code>table||=**...</code> fait des opérations matricielles entre les tables à la diférence de <code>table||=...</code><br>
c'est à dire que les calculs entre deux tables sont apliqué au cases correspondantes, avec les mêmes opérations (* implicite entre les tables autorisés), si un objet fournit n'est pas entre <code>[[]]</code> il sera automatiquementconsidéré comme tel avec une seul case, donc sera appliqué à toutes les cases de la table la plus à gauche, consernant le texte, le texte est considéré comme prioritaire au nombre et sera conservé sans modification, et si un texte se retrouve évalué avec un autre, le texte le plus à gauche sera conservé quel qu'il soit<br>
exemple 1 : <code>table|a|=**[[3;45;67;[[34;"f";6]];7;[[1;4]]]]+(5+6)</code>, table|a|--><code>[[14.00;56.00;78.00;[[45.00;"f";17.00]];18.00;[[12.00;15.00]]]]</code> car toutes les cases sont touché à tout les niveaux par +11<br>
exemple 2 : <code>table|a|=**[[3;45;67;[[34;6;"f"]];7;[[1;4]]]][[1;[[2;3]]]]</code>, table|a|--><code>[[3.00;[[90.00;135.00]];67.00;[[68.00;18.00;"f"]];7.00;[[2.00;12.00]]]]</code> car c'est une multiplication implicite entre les deux tables, les cases impair sont multiplié par 1, les cases pair qui ne sont pas en sous table sont multiplié par 2 puis par trois sur la même valeur; on voit que 45 devient <code>[[90;135]]</code>sinon le reste suit la même logique et les lettres ne sont pas touchés<br>
exemple 3 : <code>table|a|=**"momo"+[[1;3;67;[[34;"f";"kkk";4;[[34]]]];17;"c"]]</code>, table|a|--><code>[["momo";"momo";"momo";[["momo";"momo";"momo";"momo";[["momo"]]]];"momo";"momo"]]</code> car toutes les cases sont touché à tout les niveaux par un texte à gauche donc prioritaire, toutes les cases de la table on donc toute la même valeur: <code>momo</code><br></p>

<code>time_delay()</code> sans argument renvoie jourheureminuteseconde.miliseconde (en temps que nombre) seul, <code>time_delay(arg)</code> avec <code>arg</code> un ancien <code>time_delay()</code> renvoie le temps écoulé sous le même format jourheureminuteseconde.miliseconde écoulé.
<br>
<br>
<br>
dernière version stable : fr-simplecode0.3 (actuellement 12866 lignes de code C) <br> --> dernière ajout :<br><code>--arg:</code> sur la commande d'exécution permet de fournir un argument récupérable dans le code grâce à <code>[main_arg]</code>,<br>modification des règles d'appel à <code>[input]</code> fournit par <code>use_script(;)</code> sous la même forme que <code>[main_arg]</code>.
<br>
--ancienne nouveauté :<br>
fr-simplecode0.3 : 
+ ajout des variables var en texte grâce aux " " après le =
+ ajout de text_replace("arg";"arg";"arg")
+ ajout de text_cut("arg";index;index)
+ ajout de text_len("arg")
+ ajout de text_letter("arg";index)
+ ajout de text_word("arg";index)
+ ajout de text_uppercase("arg")
+ ajout de text_lowercase("arg")
+ ajout de text_eval("arg1";operateur;"arg2")
+ ajout de text_count("arg1";"arg2")
+ ajout de text_find("arg1";"arg2";index)
+ ajout de var|name|=++ et var|name|=-- pour ajouter 1 ou retirer 1

+ ajout de nano frc (! pas dans info actuelment !)
nano frc "nom_du_fichier" créer un fichier frc dans un répértoire dédier
nano delete frc "nom_du_fichier" supprime un fichier dans le répértoire si le nom donné existe
nano start frc "nom_du_fichier" démarre le script init (on peut rajouté le nom du script avec [] comme use_script()[])

+ correction de table||.len() pour les index (! il est conseillé de ne pas dépassé 3 tables imbriqué les unes dans les autres de magnière générale ! et ajout de l'ignorance system pour un index non existant dans len à 0)

+ changement des routines de table, optimisation, correction d'erreur  

+ ajout de var_local||

+ ajout de commentaire grâce à <code>$//</code> puis <code>//$</code> pour fermé

<br>
pour plus d'info : <a href="https://github.com/argentrocher/fr-simplecode/wiki">(voir wiki)</a>
<br><br>
commande de test 0.3 : <br><br>

<code>[[[{print("text(text_letter("atext(text_letter("abc";3))";2))")}]]]</code>

<code>[[[{print("text(text_uppercase("text(text_uppercase("momo"))"))")}]]]</code>

<code>[[[{print("text(text_lowercase("text(text_lowercase("momo"))"))")}]]]</code>

<code>[[[{print("text(text_cut("text(text_cut("1h1h2";-1;-2))";2;1))")}]]]</code>

<code>[[[{print("text(text_replace("text(text_replace("momo";"o";"i"))";"m";"t"))")}]]]</code>

<code>[[[{print(text_find("arg arg 234";"a";-1))}]]]</code>

<br>
commande test 0.4 : <br><br>
<code>[[[{print("num(hash("bdkfbbrvdjvrjbvjerbvberbrejbrjebzejvjbvjebvbevbhbhbvvvecvgzcyzazgcfjzebchezvchezchzvchcvchvecvehvcvhevcvechevchevcvcvhdschzeejzvbbvhbevhbhhscjbvhvbjzbvhbzvebhfvjsbchvuevhvfevzefbufbuzgecuebczbvezvezvezjhfbezgfugefygezgfezfyfceugeycuegcyzgicheugcyehuefugyuezgezgfeyfezfezxgeygxuyzgcchjegcececcezyegfygfuyezgyuegcyevxvexezvxexvejbverhrvhrevbryvgyrgyrgfyergfyergfyegfygfyhfuiezgfuzheihcuegcyuejhegczgcyuechjezgeygcegcgeucgecehckehqskhcuidgcyegchjgcsqgcyuegcuyegcyuezgcuygcugckjsqhsgcuz_675654534486879896655465766745644463435454454546465456JHUGYGYGTFTYFTFTFTFTFTFRDRTUGUKTTFFJHGJFFJGJYFYJFTFDTRHGHJJKUGYFRERT4534336467HGGHVGHFGFDFGDGHFHFHFGDTDRTDTRDGHJVHVGFDGDbdhvbhbvjvdbdjhbvjhdvyugchdgjvsddvjsdjvsjdhvgsddsgfdsjgfjfdshggcjcgjdsgcdcgdscvdscvyusdcsydgcjhgchjsgyugeyegcjhgcjssyucsycfyshchjqscyucgezychsgchgcyuegchjscfhcgcfyueycgyefcyuegcezvebehvejbhjvzexvvezsxuyevcjecyuegcyuehcuceygcegciuehcyuegcgcegcuegcuycezihfuehfugfyugetyfteyfdyuezfdyuafzdtyfefcuegctyfecegcgezeiuhfhzgyuegfegyuehxuggdygezyygexcdae"))")}]]]</code> = hash à .1000 charactères (corespond au nombre décimaux) et 11 chiffres de hashage maximum pour respecter le format du double (15 chiffres max); résultat : <code>37805820906.10</code><br><br>

<code>[[[{var_local|char|="mon char !"}{print("text(text_letters("text(var_local|char|)";text_len("text(var_local|char|)");text_find("text(var_local|char|)";"c";text_count("text(var_local|char|)";"c"))))")}]]]</code> affiche char ! (du dernier 'c' jusqu'à la fin)<br><br>

<code>[[[{var_local|a|="text(prompt("password (la vache)"))"}{if(||hash("text(text_letters("text(var_local|a|)";-1;-8))")||<=||36498736912.008|| and ||hash("text(text_letters("text(var_local|a|)";-1;-8))")||>=||36498736912.008||){print("oui")}||else||{print("non")}}]]]</code> montre qu'un mot de passe est indécodable sans le connaître réelement, même dans le code (tout se qui finit par <code>la vache</code> est <span>$oui$</span>) avec la nouvelle fonction <code>text_letters("arg";index1;index2)</code> qui renvoie plusieurs charactères en même temps à la différence de <code>text_letter("arg";index)</code>

<br>

exemple de moyenne d'une table par rapport à des oppérations random : <code>[[[{table_local|o|=[[]]}{repeat||50||{repeat||100||{var_local|o|=++}}{print("O "var_local|o|)}{table_local|o|.add(var_local|o|)}{var_local|o|=random(0;var_local|o|)}}{var_local|moy|=0}{var_local|index|=1}{repeat||table_local|o|.len()||{var_local|moy|=var_local|moy|+table_local|o|[var_local|index|]}{var_local|index|=++}}{var_local|moy|=var_local|moy|/var_local|index|}{print("la moyenne des valeurs tirés est de "var_local|moy|)}]]]</code>

<br>
<br>
comment récupérer la version de mon application ?
<br>
<code>[[[{if||version()||?=||0||{print("version : fr-simplecode0.2 ou précédente")}}{if||version()||?=||0.3||{print("version : fr-simplecode0.3")}}{if||version()||?=||0.4||{print("version : fr-simplecode0.4")}}]]]</code>
