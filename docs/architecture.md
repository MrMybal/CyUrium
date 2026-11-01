# Architecture desktop

## Périmètre actuel

CyUrium est un projet desktop indépendant de CyVR_Urium. Ce dernier conserve
son intégration GeckoView, Android/Quest, son rendu et ses fonctions immersives.
Ses sources locales ont été consultées comme référence, sans modification.

L’interface native héberge maintenant de vraies pages Gecko sous Windows.
Le logo original est copié depuis CyVR_Urium ; le projet Quest reste inchangé.
Le runtime Mozilla est embarqué dans le paquet et utilise un profil CyUrium.
Aucun Firefox installé ni moteur Chromium/WebView n’est requis.

## Composants

| Fichier | Responsabilité |
| --- | --- |
| `src/browser_model.*` | État d'une fenêtre, onglets à identifiants stables, historique ; profil partagé et sauvegarde |
| `src/browser_workspace.*` | Propriété des fenêtres et transfert des onglets entre elles |
| `src/browser_view.hpp` | Contrat de présentation, rectangles d'onglets, commandes et état visuel du déplacement |
| `src/tab_drag.*` | Seuil du grab, cible, position d'insertion, détachement et annulation |
| `src/browser_ui.*` | Présentation `classic` Dear ImGui, inspirée de Chrome |
| `src/web_engine.*` | Cycle du runtime, IPC authentifié, état web et surfaces natives Windows |
| `src/browser_sdk.*` | Compagnon SDK possédé, requêtes appliquées sur le thread UI, niveaux d’accès |
| `src/browser_assistant_ui.cpp` | Panneau natif de conversations à côté de la page |
| `sdk/` | Contrat CyTools, Runtime/MCP officiel embarqué et fournisseurs portables existants |
| `tools/prepare_sdk.py` | Runtime privé et dépendances vérifiés par SHA-256, licences de distribution |
| `engine/host/` | Hôte Gecko privilégié minimal, fenêtres de contenu et acteurs du moteur |
| `tools/prepare_gecko.py` | Runtime ESR officiel vérifié et application Gecko dédiée |
| `src/browser_theme.*` | Palettes navigateur et adaptation des rôles CyUI |
| `src/main.cpp` | Fenêtres SDL3, événements, raccourcis, hébergement des présentations et commandes GPU |
| `third_party/cyui/` | Snapshot minimal des thèmes et widgets de CyImgui_Interface |
| `cmake/Dependencies.cmake` | Archives officielles et empreintes des dépendances |

Le modèle ne dépend pas de Dear ImGui, SDL ou Gecko. L'interface utilise les
widgets de saisie ImGui, du dessin vectoriel pour les icônes et des palettes
sémantiques. CyUI apporte la gestion des thèmes/DPI, un switch et un contrôle
de taille. Son éditeur, ses outils audio et sa démonstration ne sont pas inclus.
Les polices et icônes système Windows ne sont pas nécessaires au rendu commun.

SDL3 isole les fenêtres, le presse-papiers, la saisie et les différences de
backend GPU : Direct3D 12, Metal ou Vulkan. Les ressources de rendu ImGui sont
réutilisées par le backend officiel SDL_GPU. Les captures CPU n'existent que
dans la commande de contrôle visuel, et non dans la boucle de rendu ordinaire.

## Onglets, fenêtres et futures interfaces

`BrowserWorkspace` possède les modèles des fenêtres. Chaque modèle possède
ses onglets et sa sélection ; tous partagent un `BrowserProfile` pour les
préférences, favoris et adresses saisies. Un transfert déplace l'objet `Tab`
avec son identifiant, son historique et son curseur retour/avance. Il ne recrée
pas une destination. La session Gecko et sa surface native suivent ce même identifiant.

Chaque fenêtre possède une instance de `BrowserView`. La présentation publie
les rectangles de ses onglets dans les coordonnées globales du bureau, puis
le contrôleur commun détermine le grab et la destination. La fermeture et les
transferts sont appliqués hors du dessin des vues. Le dernier onglet emporte
sa fenêtre ; les autres continuent à vivre. Les régions natives de déplacement
et de redimensionnement excluent les corps d'onglets et leurs boutons.

`BrowserUi` implémente ce contrat pour la présentation `classic`. Une nouvelle
présentation implémente `BrowserView` et s'ajoute à `make_browser_view` ; elle
peut modifier toute la disposition, tout en conservant les mêmes commandes,
modèles et interactions entre fenêtres. Le programme accepte `--interface
classic` ; aucune seconde présentation n'est encore livrée.

Le backend officiel Dear ImGui **1.92.5-docking** crée les fenêtres natives
secondaires avec les multi-viewports. Un seul contexte ImGui partage l'atlas
de polices et les ressources du renderer SDL_GPU. Les fenêtres de navigateur
sont sans parent natif, redimensionnables et présentes dans la barre des tâches.
L'aperçu du grab est une petite fenêtre sans entrée ni focus. Si la première
fenêtre est fermée, son hôte technique est masqué jusqu'à la fermeture des
dernières fenêtres. Les callbacks de hit-test sont retirés avant de libérer
leur état.

Le backend exige actuellement des coordonnées globales du pointeur : sur Linux,
X11/XWayland est nécessaire. Wayland natif est une limite explicite de ce jalon.

## Boucle et limites de performance

Le rendu est synchronisé à l'affichage pendant l'interaction. Une période
courte sert les transitions et tooltips ; au repos la boucle attend les
événements, avec un réveil pour le curseur actif. La fenêtre réduite ne soumet
aucune image. Les écritures de préférences sont différées pour ne pas écrire
sur le disque à chaque mouvement d'un slider.

`--metrics` rapporte le temps de construction CPU de l'interface, le nombre
d'images, le temps CPU total du processus et sa mémoire. Le temps GPU, les
performances web et la latence de bout en bout ne sont pas déduits de ces valeurs.
Les événements du moteur réveillent la boucle via SDL ; ses surfaces natives
se composent indépendamment de l’interface.

## Gecko desktop sous Windows

Le runtime est Gecko ESR **153.6.0**, extrait de la distribution officielle
Firefox ESR Windows x64, vérifiée par SHA-512 dans `engine/runtime.lock.json`.
Son lanceur Mozilla (`engine/firefox.exe`) démarre `engine/app/application.ini`
avec `-app`, notre identifiant et notre profil. Les ressources de la plateforme
Mozilla sont conservées ; l’interface Firefox n’est pas utilisée. L’application
hôte et la recette d’assemblage sont livrées dans `engine/host-source/`.

Le manager XUL démarre hors écran. Une fenêtre Gecko contenant un élément
`browser` distant est créée par identifiant d’onglet. Un canal TCP exclusivement
sur 127.0.0.1, à port aléatoire et authentifié par défis HMAC mutuels, transporte
les commandes JSON et les notifications. Ce canal est accessible au code
privilégié de l’hôte ; les pages HTTP ne reçoivent ni jeton, ni API native.
Les chargements acceptent HTTP, HTTPS, les fichiers et les origines
moz-extension enregistrées par une extension installée. Les contrôles TLS
et le sandbox de contenu Gecko ne sont pas désactivés.

Le pont Windows transforme la fenêtre Gecko en enfant de la fenêtre SDL et
adapte ses dimensions à la zone de contenu. Il la masque quand l’onglet n’est
pas affiché. Avant la destruction d’une fenêtre source, la surface survivante
est temporairement détachée ; elle est ensuite rattachée au nouveau parent.
Le document, son processus et son formulaire restent vivants. Les popups ImGui
utilisent des fenêtres natives pour ne pas être masquées par le contenu web.

Un groupe de processus Windows suit le runtime et ses descendants. La fermeture
de CyUrium demande un arrêt à Gecko, puis ferme ce groupe. Les événements de
navigation du moteur alimentent l’URL, le titre, les commandes retour/avance,
le chargement et les erreurs du modèle ; ils ne passent pas par une nouvelle
navigation artificielle. Les raccourcis de l’hôte web sont renvoyés au shell.

`--web-test` active des diagnostics sur la fixture loopback,
pour comparer l’instance du document et la valeur d’un formulaire. La capture
Gecko n’existe que dans ce test. Le rendu ordinaire n’effectue aucune lecture
des pixels web vers le CPU.

Le backend natif reste à porter sur macOS/Linux. GeckoView et le pont
JNI/AHardwareBuffer du projet Quest restent propres à Android.

## Validation et limites

Les tests du modèle et les tests d'interaction ImGui s'exécutent sans afficher
de fenêtre. Les captures de contrôle passent par le backend GPU de l'application.
La matrice GitHub Actions est préparée pour compiler/tester les trois OS ; elle
n'a pas encore été exécutée dans ce projet local. Les vérifications effectives
de cette première étape sont sous Windows.

Les onglets détachables et les fenêtres multiples sont vérifiés sous Windows
en Release et Debug, y compris la survie d'une fenêtre secondaire après la
fermeture de la première. Les sessions, téléchargements, permissions courantes
et extensions signées disposent de services dédiés. L’accessibilité, les API
liées à l’interface Firefox, la composition de textes complexes et le plein écran
HTML/vidéo restent à porter ou à valider. Dear ImGui ne fournit pas, à lui
seul, toutes les fonctions d'un toolkit d'interface grand public ; ces besoins
devront être traités explicitement avant une distribution complète.

Références : [Dear ImGui](https://github.com/ocornut/imgui),
[SDL_GPU](https://wiki.libsdl.org/SDL3/CategoryGPU),
[GeckoView](https://mozilla.github.io/geckoview/),
[sources Mozilla](https://github.com/mozilla-firefox/firefox).


## Services natifs, sessions et mise à jour (0.3)

`BrowserServices` partage les états et commandes entre toutes les vues. Le canal
privilégié publie les listes de Downloads et AddonManager ainsi que les demandes
d’autorisation. Les pages web n’accèdent jamais à ce canal. Le prompt XPCOM est
remplacé dans notre application uniquement ; les acteurs multimédias utilisent
la validation et les périphériques Gecko. Les demandes sont annulées à la
navigation, à la fermeture et après expiration. La signature des extensions
reste obligatoire. Les panneaux d’extension ont leur propre browser distant de
type popup, sans remplacer l’onglet sélectionné du navigateur.

L’adaptateur d’extensions s’installe dans le gestionnaire d’API après son
initialisation. Il associe les fenêtres Gecko aux identifiants du workspace SDL.
Les API qui supposent un tabbrowser Firefox complet ne sont pas toutes portées.

`SessionStore` valide la totalité du fichier avant de modifier le workspace,
borne les tailles et écrit par remplacement atomique avec copie de secours.
Sous Windows, le JSON de version 2 est enveloppé par DPAPI pour le compte
utilisateur, y compris le secours. Une ancienne session JSON est migrée avec
retrait de ses champs avant réécriture chiffrée. Une sauvegarde corrompue ne
remplace pas un secours valide. Le workspace reste inchangé si la validation ou
la migration échoue.
Les formulaires sont exclus par défaut ; l’option Navigation permet leur
restauration sous Windows. Le parent impose la politique à l’acteur de contenu.
Mots de passe, champs cachés, fichiers, types courriel/téléphone, données de paiement,
OTP et champs identifiés comme secrets sont exclus même après activation.
Ce filtrage est une réduction de collecte, sans prétention de reconnaissance
universelle des données sensibles. La restauration exige l’URL exacte. Les autres
plateformes ne persistent pas les champs tant qu’aucun stockage protégé n’est
implémenté. La boucle ne sauvegarde pas à chaque image rendue.

`LocalBridge` lie exclusivement une socket loopback. Un secret BCrypt de 256 bits
reste dans les processus parents. Le défi, le nonce client, le PID, la version et
les capacités annoncées forment un transcript HMAC-SHA-256 avec rôles client et
serveur distincts. Les nouvelles connexions ont un nouveau défi. Les queues sont
bornées, le préambule est limité à 8 Kio/2 secondes et la profondeur JSON est
bornée avant analyse. Gecko vérifie le serveur avant d’initialiser les services
ou d’exécuter les commandes. Son environnement est ensuite débarrassé du secret.
Les titres ne contiennent qu’un identifiant indépendant ; EnumWindows vérifie
également le PID. Un refus ferme les flux ; le parent possède le job et termine
les processus lors d’un échec fatal, y compris si le démarrage Gecko attend une
barrière de fermeture. Ce canal n’isole pas d’un attaquant contrôlant le compte
Windows et pouvant lire la mémoire du parent.

Le démarrage exige multiprocessus, Fission et un confinement effectif positif.
Les variables héritées désactivant les sandbox Gecko sont retirées de
l’environnement de l’enfant. SafeBrowsing est explicitement initialisé, avec
les listes standard du moteur. L’état TLS transmis par nsIWebProgressListener
alimente l’indicateur natif et l’origine affichée. Une navigation en attente ne
réutilise pas l’indication de la page précédente. Un certificat refusé ou une
page classifiée dangereuse n’a pas de bouton natif de contournement.

La recette Gecko vérifie le SHA-512 de l’archive verrouillée, liste ses fichiers
et contrôle les extractions par taille/CRC avant empreinte SHA-256. Elle copie
uniquement ces entrées et les fichiers d’hôte générés. `runtime-files.json`
enregistre l’assemblage. La publication vérifie toutes ces empreintes et refuse
les ajouts étrangers. `release.py stage` crée un dossier neuf depuis la liste
approuvée, sans supprimer les intégrations de l’antivirus du runtime local.
Cette provenance suppose un poste/runner de compilation de confiance.

La récupération du moteur détruit son ancien groupe de processus, recrée le
canal authentifié et les surfaces, puis recharge les URL et états admissibles.
Le compteur limite les relances rapprochées. L’accélération est configurée avant
le démarrage du moteur et son état effectif provient de `nsIGfxInfo`.

Le canal de mise à jour embarque une clé publique et un dépôt public GitHub
Releases, jamais de clé privée ni de jeton. Le canal stable utilise la dernière
Release complète ; le canal preview choisit la plus haute version numérique
parmi les 30 dernières publications compatibles, hors brouillons. Les URL des
assets sont liées au dépôt et au tag. Les requêtes sont anonymes et les
redirections vers HTTP sont refusées à chaque étape. Le rythme automatique est
quotidien, avec horodatage persistant et respect des limites API. Le manifeste
HTTPS direct reste disponible pour les anciens canaux.
WebCrypto vérifie la signature du manifeste ; un SHA-256 lie le ZIP
à ce manifeste. L’extraction refuse les chemins ambigus ou sortant du staging.
Un helper copié hors installation attend la fermeture du navigateur, vérifie
chaque fichier, remplace le dossier et attend un signal de santé du shell et des
services Gecko. Un échec provoque un retour automatique à l’ancienne version.
La version affichée et celle du signal de santé proviennent de CMake. Un outil
de configuration synchronise le canal, cette version et celle de l’hôte Gecko.
Un workflow manuel prépare le paquet signé et peut créer un brouillon de Release.
La procédure est décrite dans [releases.md](releases.md).
Les tests de transport utilisent HTTP loopback exclusivement en mode QA, car
l’antivirus local intercepte le certificat de la fixture HTTPS ; cette exception
n’est accessible ni aux pages ni au canal de production.
