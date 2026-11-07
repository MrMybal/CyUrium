# CyUrium 0.3.0 — Alpha

[README](README.fr.md) · [English](VERSION.md) · [Suivi des problèmes](https://github.com/MrMybal/CyUrium/issues)

## Disponibilité

Ces notes décrivent la **version 0.3.0 des sources** et l’implémentation Windows
de ce projet. Aucun paquet de l’application n’est encore publié dans les
[Releases GitHub](https://github.com/MrMybal/CyUrium/releases).
Cette alpha est expérimentale et destinée aux essais et aux retours utilisateurs.

Windows x64 héberge Gecko et le runtime privé conversation/SDK. macOS et Linux
disposent actuellement de cibles de compilation de l’interface ; leur intégration
web native reste à porter. Android/Meta Quest relève du projet distinct CyVR_Urium.

## Fonctions incluses

- Interface native inspirée de Chrome, thèmes clair/sombre, mise à l’échelle,
  favoris et historique des adresses saisies.
- Onglets web indépendants avec déplacement/réorganisation, détachement/rattachement
  entre fenêtres natives et conservation du document, du formulaire et de l’historique.
- Navigation Gecko HTML/JavaScript, téléchargements, extensions signées et uBlock Origin embarqué.
- Permissions par origine, sauvegardes des sessions, restauration des champs sur option et récupération du moteur.
- Panneau de conversation natif, connecteurs fournisseurs existants et accès MCP
  activé séparément via CyToolsCore.
- Rendu d’interface SDL_GPU et accélération Gecko/WebRender sous Windows.
- Implémentation des mises à jour GitHub signées, vérification des fichiers et
  retour à l’ancienne installation après échec du démarrage ; la clé de publication
  et le dépôt dédié aux Releases restent à configurer.

## Composants verrouillés

| Composant | Version | Rôle |
| --- | --- | --- |
| CyUrium | 0.3.0 · Alpha | Version des sources de l’application. |
| Gecko | 153.6.0 ESR | Runtime web Windows. |
| Dear ImGui / SDL | 1.92.5-docking / 3.2.28 | Interface native de bureau. |
| uBlock Origin | 1.75.0 | Extension embarquée signée par Mozilla. |
| CyToolsCore / CyUEMCPSDK | 0.10.1 / 0.1.8 | Runtime SDK et snapshot des fournisseurs. |
| CPython / MCP Python SDK | 3.14.10 / 1.30.0 | Dépendances privées du SDK Windows. |

Ces versions proviennent de CMake et des verrous du moteur et du SDK. Elles
décrivent les sources présentes ; aucun binaire n’est encore publié.

## Limites documentées

- **Plateformes** : Gecko, le compagnon SDK empaqueté et les mises à jour natives
  restent limités à Windows. L’exécution macOS/Linux n’a pas été validée localement.
  Le placement multifenêtre Linux exige X11/XWayland ; Wayland natif n’est pas pris en charge.
- **Extensions** : certaines API dépendantes de l’interface Firefox restent à
  adapter, dont des créations d’onglets/fenêtres, barres latérales et menus
  contextuels. Toutes les extensions Firefox ne sont pas compatibles.
- **Compatibilité web** : plein écran HTML/vidéo, accessibilité, diversité des sites
  et périphériques de capture demandent encore une validation élargie. La présence
  d’un gestionnaire de permissions ne garantit pas la compatibilité complète.
- **Confidentialité des sessions** : la restauration des champs est désactivée par
  défaut et disponible sous Windows. Les mots de passe, fichiers, champs cachés,
  données de carte bancaire reconnues et codes à usage unique sont exclus.
  DPAPI protège les sessions et leur secours pour le compte Windows ; les réglages,
  l’historique des adresses saisies et tout le profil Gecko ne sont pas chiffrés par ce mécanisme.
- **Récupération** : trois relances automatiques rapprochées maximum sont tentées ;
  une relance manuelle reste possible. La restauration peut reprendre l’activité des pages.
- **Connecteurs IA** : des outils CLI/comptes installés ou un fournisseur API/local
  configuré sont nécessaires. Les outils navigateur de Codex App Server utilisent
  un protocole expérimental de fonctions dynamiques qui exige une version compatible.
  Les autres connecteurs assurent la conversation ; la page active doit être jointe explicitement.
- **Mises à jour** : aucun téléchargement n’est actuellement proposé. Le dépôt
  public des Releases et la véritable clé du mainteneur ne sont pas renseignés ;
  rotation de clé et suffixes de préversion ne sont pas implémentés.
- **Performances** : les mesures de l’interface excluent les processus Gecko et
  la mémoire GPU. Aucun comparatif de navigation multiplateforme n’est annoncé.

## Problèmes et retours

Aucune liste de corrections détaillée par problème n’est publiée pour 0.3.0.
Les limites ci-dessus décrivent les prises en charge incomplètes et les validations
restantes. Signalez les problèmes reproductibles dans le
[suivi des problèmes](https://github.com/MrMybal/CyUrium/issues), avec la plateforme,
les étapes et le résultat attendu. Retirez le contenu personnel des pages avant
de joindre des fichiers.

Consulter l’[architecture](docs/architecture.md), le [guide SDK](docs/sdk.md) et
le [guide de publication](docs/releases.md) pour les détails techniques en français.
