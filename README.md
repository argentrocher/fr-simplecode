fr-simplecode est un language basique de codage pour windows (64 bits uniquement), découvrer le ! 

dernière version : fr-simplecode0.3 (! en développement, le fichier change régulièrement !) (actuellement 12008 lignes de code C) <br> --> correction de bug, pas de nouveauté


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

[[[{print("text(text_letter("atext(text_letter("abc";3))";2))")}]]]

[[[{print("text(text_uppercase("text(text_uppercase("momo"))"))")}]]]

[[[{print("text(text_lowercase("text(text_lowercase("momo"))"))")}]]]

[[[{print("text(text_cut("text(text_cut("1h1h2";-1;-2))";2;1))")}]]]

[[[{print("text(text_replace("text(text_replace("momo";"o";"i"))";"m";"t"))")}]]]

[[[{print(text_find("arg arg 234";"a";-1))}]]]
