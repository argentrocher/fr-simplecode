fr-simplecode est un language basique de codage pour windows (64 bits uniquement), découvrer le ! 

dernière version : fr-simplecode0.3 (! en développement, le fichier change régulièrement !) (actuellement 9728 lignes de code C)

fr-simplecode0.3 : 
+ ajout des variables var en texte grâce aux " " après le =
+ ajout de text_replace("arg";"arg";"arg")
+ ajout de text_eval("arg1";operateur;"arg2")
+ ajout de var|name|=++ et var|name|=-- pour ajouter 1 ou retirer 1

+ ajout de nano frc (! pas dans info actuelment !)
nano frc "nom_du_fichier" créer un fichier frc dans un répértoire dédier
nano delete frc "nom_du_fichier" supprime un fichier dans le répértoire si le nom donné existe
nano start frc "nom_du_fichier" démarre le script init (on peut rajouté le nom du script avec [] comme use_script()[])

+ correction de table||.len() pour les index (! il est conseillé de ne pas dépassé 3 tables imbriqué les unes dans les autres de magnière générale !)
