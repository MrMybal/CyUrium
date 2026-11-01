# Validation Windows — 2026-11-01

Licence du projet définie le 1 novembre 2026 : `GPL-3.0-only`, texte intégral
dans `LICENSE` et périmètre dans `NOTICE`, copyright Cyberalien 2026. Mentions
SPDX ajoutées à 86 fichiers propres au projet ; les composants tiers conservent
leurs licences. Les paquets Windows Release et Debug incluent les trois fichiers
`LICENSE`, `NOTICE`, `THIRD_PARTY_NOTICES.md`. Les cinq CTests de chaque
configuration réussissent. Les quatre tests de fabrication vérifient notamment
la conservation des notices dans le staging et le ZIP signé de contrôle, ainsi
que le refus d’un paquet sans licence. Aucune publication de sources ou de
binaires n’a été effectuée ; les sources correspondantes devront être mises à
disposition lors des futures distributions publiques.

uBlock Origin intégré par défaut, 1 novembre 2026 : paquet officiel Mozilla
1.75.0 inchangé, version et SHA-256 figés dans
`engine/bundled-extensions.lock.json`. L’installation est locale et la signature
est vérifiée par Gecko. Le code source amont, les listes exactes et les licences
sont fournis avec l’extension. Les profils existants sans uBlock le reçoivent ;
une copie déjà installée, même désactivée, est conservée sans remplacement.

`tools/test_adblock.py` vérifie dans de vrais processus Gecko que la première
page charge son script normal tandis que le script `/ads/custom_ads.js`, visé
par la liste EasyList intégrée, n’atteint jamais le serveur local de test.
Le parcours redémarre ensuite le même profil : migration avec uBlock désactivé,
réactivation, puis désinstallation, avec filtrage vérifié dans chaque état.
CyUrium attend la disponibilité réelle des filtres, au-delà de la seule
activation de l’extension. Le contrôle lit le signal de disponibilité du tableau
de bord officiel dans un navigateur masqué et temporaire, sans modifier le XPI
ni exécuter de scripts injectés. Les confirmations pour les XPI choisis par
l’utilisateur restent requises. Voir [rapport](validation/adblock.json).

Paquets Release et Debug reconstruits, cinq CTests réussis pour chacun.
Filtrage réel et conservation des choix réussis dans les deux configurations.
Le parcours web natif Release, y compris les pages internes séparées et la
récupération après arrêt du moteur, réussit avec uBlock préinstallé. Le test
d’installation manuelle vérifie toujours la confirmation des autorisations,
les identifiants d’onglets/fenêtres et le véritable panneau de l’extension.
Les quatre tests de fabrication des Releases réussissent ; le XPI, sa licence
et son archive de sources figurent dans les inventaires de provenance des deux
paquets. Une copie de cache altérée est refusée avant l’assemblage.

Assertion de mise en page ImGui corrigée, 1 novembre 2026 : le problème a été
reproduit dans `interface_interactions` Debug avec les listes remplies. Les
cartes de favoris/historique déclarent désormais leur espacement dans la mise
en page ImGui au lieu de déplacer le curseur après le dernier élément. Les
assertions restent actives. Le test couvre les listes remplies, le défilement,
les fenêtres étroites et les échelles 85 %, 100 %, 125 % et 150 %.

Correction des pages internes, 1 novembre 2026 : paramètres, favoris,
historique, téléchargements et extensions s’ouvrent dans de nouveaux onglets,
y compris depuis leurs adresses `cyurium://`. Les tests du modèle et des boutons
vérifient la séparation des identifiants et historiques. Le parcours Gecko réel
ouvre puis ferme les cinq pages et contrôle que la surface web d’origine est
masquée pendant leur affichage, puis réaffichée avec le même document et les
mêmes champs, sans rechargement. La vidéo reste associée à son onglet d’origine ;
aucune pause automatique des médias des autres onglets n’a été ajoutée.

Intégration MCP/SDK et panneau de conversation, 1 novembre 2026 : voir
[capacités et limites](sdk.md), [MCP/SDK réel](validation/sdk.json) et
[conversation native](validation/assistant.json). CyToolsCore 0.10.1 MIT et les
connecteurs portables CyUEMCPSDK 0.1.8 sont livrés avec Python privé.
Release et Debug : cinq CTests réussis ; parcours web Release réussi après
intégration. Le MCP officiel pilote de vrais onglets Gecko. Le panneau natif
échange deux tours progressifs avec le transport Ollama existant sur un serveur
local de test, dont un avec une vraie page jointe. Annulation, isolation entre
conversations et fonctions dynamiques Codex vérifiées par fixtures du protocole ;
aucun appel à un fournisseur payant ou compte réel n’est revendiqué.

Sauvegarde avant intégration, projet sans dépôt Git :
`.cache/backups/before-cytools-20261101-005944.zip`, SHA-256
`f9c8c3ef0a8de0ff317da8bcfc196c40316fdfd7272b8627883cfc4ee7780af1`.

## Résultat

- Compilation Release MSVC x64 réussie, runtime C++ statique.
- Paquet exécuté depuis `dist/windows/CyUrium/CyUrium.exe`, backend Direct3D 12.
- Cinq suites Release réussies : modèle, fenêtres/glisser-déposer, sessions, sécurité IPC et interactions ImGui.
- Cinq suites Debug réussies, avec les assertions ImGui actives.
- Essai natif Direct3D 12 réussi en Release et Debug avec plusieurs fenêtres OS.
- Gecko ESR 153.6.0 : parcours web réel réussi en Release et Debug.

Les tests couvrent les onglets indépendants, la fermeture du dernier onglet,
l'historique retour/avance et ses branches, l'encodage UTF-8 des recherches,
les favoris, les fichiers de préférences avec chemin accentué et les données
invalides. Les tests d'interface simulent les clics et frappes dans Dear ImGui :
nouvel onglet, saisie/validation d'adresse, retour/avance, menu, thème et switch
CyUI. Le dessin est également vérifié à 150 % dans une fenêtre de 760 × 620.

La suite du workspace vérifie les identifiants uniques, le réordonnancement,
le transfert avec historique complet, le partage des préférences, la fermeture
indépendante, le seuil du grab, Échap, les boutons de fermeture et les fenêtres
qui masquent une barre d'onglets. Le bouton de fermeture d'onglet est également
testé dans l'interface, afin de vérifier que le corps de l'onglet n'intercepte
pas son clic.

`--window-test` utilise les zones réellement publiées par la présentation et
injecte pression/déplacement/relâchement dans l'entrée ImGui de l'application.
Il vérifie les effets dans le workspace et les fenêtres SDL effectivement créées :
réorganisation, détachement vers un identifiant natif distinct sans parent,
fenêtre redimensionnable, rattachement du dernier onglet d'une fenêtre,
conservation de l'identifiant et de l'historique retour/avance, puis fermeture
de la première fenêtre pendant que la seconde reste visible. La fermeture de
la dernière fenêtre termine le test. Rapport :
[native-windows.json](validation/native-windows.json).

## Web réel — version 0.3.0 alpha

`python tools/test_web.py` et `python tools/test_web.py --debug` lancent le
paquet autonome avec un profil de test isolé et une fixture HTTP loopback.
Le test vérifie HTML/CSS, JavaScript, la valeur d’un formulaire et l’identifiant
aléatoire de l’instance du document. Retour, avance puis retour utilisent
l’historique Gecko réel : le formulaire et le document restent identiques.

Le même onglet est détaché de la première fenêtre, puis du dernier onglet d’une
fenêtre secondaire, avant d’être rattaché à une nouvelle fenêtre. Le test vérifie
le parent Win32 réel de la surface Gecko et relit le formulaire après chaque
transfert. Il vérifie aussi que le menu crée une fenêtre native distincte, puis
charge `https://example.org/` en HTTPS, provoque un arrêt réel du moteur,
puis vérifie la récupération automatique de la page.

Rapports : [Release](validation/web-release.json),
[Debug](validation/web-debug.json). La [capture du contenu](screenshots/web-content.png)
provient de `WindowGlobalParent.drawSnapshot` après le dernier rattachement ;
elle n’est pas une maquette. Les pixels sont lus uniquement pour ce contrôle.

L’archive Mozilla officielle est vérifiée par SHA-512. Le processus de contenu
est séparé du shell. Ces essais ne constituent pas une validation de tous les sites et du multimédia.
Les services font l’objet des tests séparés ci-dessous.
Les tests automatiques se déroulent hors du bureau visible pour ne pas mêler
les interactions manuelles au scénario.

Le logo [PNG original](../assets/branding/urium-logo.png) est identique à celui
copié de CyVR_Urium. Ses variantes GPU et ICO sont dérivées de ce seul fichier.

## Contrôle visuel

Les captures proviennent du rendu GPU du paquet Release :

- [Accueil clair](screenshots/light.png)
- [Accueil sombre](screenshots/dark.png)
- [Paramètres](screenshots/settings.png)
- [Menu](screenshots/menu.png)
- [Petite fenêtre](screenshots/narrow.png)
- [Fenêtre d'origine après détachement](screenshots/windows-first.png)
- [Fenêtre native contenant l'onglet détaché](screenshots/windows-detached.png)

Les vues de destination non chargée et de favoris ont aussi été lancées lors
du contrôle local. L'interface adapte le contenu par défilement lorsque la
fenêtre est petite ou que l'échelle est augmentée.

## Mesure locale

Relevé de la version précédente de l’interface seule, avant intégration web,
sur un lancement de huit secondes sans interaction après ouverture :

| Mesure | Valeur observée |
| --- | --- |
| Construction CPU moyenne de l'interface | 0,344 ms par image rendue |
| Maximum, incluant l'initialisation des glyphes | 10,869 ms |
| Temps CPU total du processus sur huit secondes | 93,750 ms |
| Mémoire résidente en fin de mesure | 93,3 Mio |
| Images soumises, démarrage compris | 72 |

Après la courte période active de démarrage, l'application attend les
événements. Ces chiffres sont une observation sur cette machine : ils ne
mesurent ni le temps GPU ni la navigation. Ils ne comprennent aucun processus Gecko.
La résolution du compteur de temps CPU Windows limite la précision de la
mesure du processus. Le relevé brut est dans `validation/windows-ui.json`.

## Autres plateformes

CMake, la séparation SDL3 et la matrice CI sont préparés pour l’interface macOS/Linux.
Le pont Gecko natif de cette version est implémenté uniquement pour Windows.
Ni ces compilations ni leur rendu n'ont été vérifiés sur leurs systèmes dans
cette session.
Le multifenêtrage Linux dépend actuellement de X11/XWayland ; Wayland natif
n'est pas pris en charge pour cette fonction.

## Services et récupération — 0.3.0

- `tools/test_services.py` : vraie demande de géolocalisation, refus mémorisé,
  suppression de règle, demande du microphone, autorisation d’un périphérique
  factice, indicateur de capture et révocation native, pièce jointe réelle,
  comparaison des octets, retrait de la liste
  et diagnostic GPU Gecko.
- `tools/test_services.py --extensions` : XPI uBlock Origin signé par Mozilla,
  confirmation des droits, onglets et identifiants de fenêtres lus à travers
  l’API Gecko, véritable browser de panneau HTML, activation/désactivation et
  désinstallation. Le XPI de contrôle est téléchargé depuis Mozilla si absent.
- `tools/test_session.py` : arrêt brutal du processus de l’application, marqueur
  d’arrêt inattendu, réouverture de deux fenêtres natives et du même onglet,
  restauration du champ du formulaire après activation explicite, snapshots
  protégés par le compte Windows et absence de champs dans le réglage par défaut.
- `tools/test_updater.py` : remplacement effectif de dossiers d’installation QA,
  échec de démarrage provoqué avec retour à l’ancien exécutable, paquet altéré
  rejeté avant remplacement.
- `tools/test_update_channel.py` : API GitHub Releases simulée, redirections des
  manifestes et ZIP, signatures P-256, empreintes, cohérence tag/version et asset,
  refus de rétrogradation, préversions et brouillons, limite API, requêtes sans
  cookies, contrôle quotidien et conservation d’une version préparée. ZIP altéré
  de même taille, réponse trop grande, chemin sortant du staging et nouvelle
  tentative après rejet sont aussi vérifiés. Le serveur
  de fixture utilise HTTP sur 127.0.0.1 dans le mode QA exclusivement ; la fixture
  HTTPS avec certificat privé était interceptée par l’antivirus. La production
  exige HTTPS. Aucun magasin de certificats système n’est modifié.
- `tests/test_release.py` : CLI de génération et configuration sur une copie
  jetable, signature vérifiée indépendamment, empreintes du ZIP et de chaque
  fichier, rejet de versions/dépôts/clés incohérents et de profils à distribuer.
  Le workflow de publication est préparé mais n’a pas été exécuté sur GitHub.

## Durcissement du 1er novembre 2026

- `privileged_bridge_security` : vecteur HMAC connu, défi neuf, refus d’un faux
  client, du rejeu, des commandes avant preuve, d’un JSON trop profond/long et
  d’une connexion muette ; les commandes attendent l’authentification.
- `tools/test_bridge_peer.py` : le vrai Gecko ferme le canal devant une commande
  prématurée ou une preuve serveur invalide. Aucune création d’onglet ni état de
  service n’est émis. Les titres de ses fenêtres ne révèlent pas le secret.
  Le parent de test récolte les processus, comme le job du shell natif.
- `session_recovery` : DPAPI, secours, validation sans mutation, migration d’un
  ancien JSON et retrait de ses valeurs de formulaire, y compris dans le secours.
- `tools/test_security.py` : confinement effectif Windows niveau 9 et Win32k
  lockdown observés, Fission actif, iframe intersite dans un autre processus,
  accès DOM interorigine refusé, absence de globals privilégiés dans la page,
  collecte des formulaires désactivée puis activée avec exclusion des champs
  sensibles, CSP appliquée, connexion HTTPS reconnue, certificat auto-signé non
  approuvé refusé par Gecko (`about:certerror`), et démonstration antiphishing
  inoffensive de Mozilla bloquée (`about:blocked`). Aucun certificat ajouté aux
  magasins de confiance.
- `tests/test_release.py` : fichier étranger refusé, staging approuvé sans cet
  ajout et sans modification du runtime source, puis fichier altéré refusé.
- `tools/check_gecko.py` : comparaison en lecture seule avec le service officiel
  Mozilla ; le verrou 153.6.0esr correspondait à l’ESR annoncée lors du contrôle.

Les niveaux de confinement ci-dessus décrivent cette machine, pas une exigence
universelle chiffrée pour tous les systèmes. L’application exige un niveau
effectif positif. Le chiffrement de session ne couvre pas le reste du profil et
ne bloque pas un logiciel malveillant disposant des droits du même compte.

Les scripts utilisent des profils isolés. Le test de permissions rend sa propre
fenêtre visible car Gecko suspend la localisation d’un document masqué. Les
autres tests natifs se déroulent hors écran.

L’accélération des pages est observée avec WebRender sur le GPU de test. Les détails
Gecko indiquent séparément les capacités disponibles, bloquées et indisponibles,
y compris le décodage vidéo : activer la préférence ne vaut pas validation de
chaque codec. Les essais ne certifient ni toutes les WebExtensions, ni les API
spécialisées de permissions, ni la compatibilité macOS/Linux du moteur web.
