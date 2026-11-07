# Distribution depuis GitHub Releases

## Alpha Windows installable

La préversion [0.3.0 Alpha 1](https://github.com/MrMybal/CyUrium/releases/tag/v0.3.0-alpha.1)
est distribuée dans le dépôt public du projet : installateur Windows x64,
ZIP portable, archive des sources correspondantes et `SHA256SUMS`.
L’installateur Inno Setup propose français/anglais, installe pour le compte
courant sans élévation et crée les raccourcis et la désinstallation Windows.
La désinstallation conserve le profil situé hors du dossier de l’application.

Cette alpha utilise des téléchargements manuels. L’installateur ne porte pas de
signature Authenticode et aucun manifeste de mise à jour automatique n’est publié.
La clé de publication différée n’est pas générée par la préparation de l’installateur.

Après compilation et tests, avec le compilateur officiel Inno Setup 7 :

```powershell
python tools/build.py
python -m unittest discover -s tests -p test_windows_package.py
python tools/package_windows.py --iscc 'C:\Program Files\Inno Setup 7\ISCC.exe' --output dist/releases/alpha
python tools/package_source.py --output dist/releases/alpha
```

Le compilateur utilisé pour Alpha 1 est verrouillé dans
`packaging/windows/compiler.lock.json`. Vérifier son empreinte et sa signature
officielle avant son exécution. Le script de paquet crée un staging neuf, conserve
uniquement les fichiers Gecko approuvés et contrôle les profils et chemins locaux.
Il refuse d’écraser un jeu de paquets existant. L’archive des sources provient du
commit Git et ajoute les archives officielles verrouillées de SDL, Dear ImGui et
uBlock Origin. Les bibliothèques déjà vendues dans le dépôt restent incluses.

## Canal de mises à jour signées

La distribution utilise un **dépôt public dédié aux Releases**. Le dépôt du code
peut rester privé. Aucun compte GitHub, jeton ou outil Python n’est nécessaire
sur les machines qui exécutent le navigateur. Le nom du dépôt public n’est pas
encore renseigné dans cette copie du projet.

## Fonctionnement du client Windows

`assets/update-channel.json` contient le dépôt `propriétaire/nom`, le canal,
le nom du manifeste de plateforme et la clé publique du mainteneur.

- `stable` consulte l’API `/repos/{owner}/{repo}/releases/latest` et refuse les
  brouillons et préversions.
- `preview` examine les 30 dernières Releases, ignore les brouillons et choisit
  la version numérique la plus élevée contenant le manifeste Windows. Une
  préversion porte aussi un numéro `major.minor.patch` et le drapeau GitHub
  « Pre-release » ; les suffixes `-beta` ne sont pas encore gérés.
- Le tag doit être `v0.3.1` ou `0.3.1`, pour un manifeste signé de version `0.3.1`.
  Une version inférieure ou égale à celle installée n’est jamais proposée.
- Une vérification automatique est programmée après le démarrage du moteur web,
  puis au plus une fois par jour. Son horodatage est conservé dans le profil.
  `auto_check: false` désactive cette programmation. La vérification manuelle
  reste disponible dans **Paramètres > Système**.
- Le téléchargement et le redémarrage restent déclenchés par l’utilisateur.
  Une version déjà préparée garde son bouton d’installation. Une limite de
  requêtes GitHub suspend les vérifications pendant le délai annoncé.

La signature du manifeste est vérifiée en ECDSA P-256/SHA-256. Elle lie la
version, la plateforme, l’URL, la taille et l’empreinte SHA-256 du ZIP. L’URL et
la taille doivent également correspondre à un asset de la même Release. Les
redirections des assets GitHub sont prises en charge ; chaque étape exige HTTPS
et les requêtes n’envoient pas les cookies du navigateur.

Le ZIP contient le navigateur entier et Gecko. Après extraction contrôlée, le
helper vérifie chaque fichier, attend la fermeture du navigateur et remplace
l’installation. Le nouveau shell et les services Gecko doivent démarrer pour
valider l’installation. Sinon, le helper remet l’ancienne installation en place
et la relance avec le même profil. La dernière erreur d’installation est affichée
dans les paramètres. L’ancienne installation reste dans un dossier de secours
adjacent ; aucune suppression automatique de ces sauvegardes n’est effectuée.

La mise à jour native reste limitée à **Windows x64**. Le nom des manifestes
laisse de la place aux autres plateformes, dont le moteur natif reste à porter.

## Préparer les clés et le premier paquet

Installer la dépendance réservée au mainteneur :

```powershell
python -m pip install -r tools/requirements-release.txt
python tools/release.py keygen --key 'D:\Keys\CyUrium\release-private.pem'
```

La commande affiche `public_key_spki`. Conserver la clé privée hors du dépôt et
la même clé pour les versions successives ; la rotation de clé n’est pas gérée.
Le client embarque uniquement la clé publique.

Après création du dépôt public, initialisé par exemple avec un README :

```powershell
python tools/release.py configure --repository VOTRE_COMPTE/CyUrium-Releases --key 'D:\Keys\CyUrium\release-private.pem' --channel stable --version 0.3.1
python tools/build.py
python tools/release.py stage --output dist/release-source/CyUrium
python tools/release.py package --source dist/release-source/CyUrium --key 'D:\Keys\CyUrium\release-private.pem' --repository VOTRE_COMPTE/CyUrium-Releases --version 0.3.1
```

`configure` écrit le canal public et synchronise la version CMake, celle du moteur
et son BuildID. Il accepte aussi `--public-key <SPKI-base64>` à la place de
`--key`, ce qui permet de compiler sans accès à la clé privée. L’affichage du
navigateur et le signal de santé utilisent la version CMake compilée.

`package` refuse une version différente du binaire préparé, une clé publique ou
un dépôt embarqué différents de ceux annoncés, et les profils/liens présents
dans le dossier à distribuer. Les fichiers produits dans `dist/releases/` sont :

| Fichier | Usage |
| --- | --- |
| `CyUrium-0.3.1-windows-x64.zip` | Navigateur portable complet |
| `latest-windows-x64.json` | Manifeste signé utilisé par le client |
| `SHA256SUMS` | Empreintes pour contrôle manuel |

Joindre ces trois fichiers à la Release `v0.3.1` dans le dépôt public. Sur le
canal stable, cette Release doit être une publication complète et être la
dernière Release selon GitHub. Les paquets n’incluent aucun profil utilisateur.
Le nom `latest-windows-x64.json` est propre à chaque Release, sans fichier à
modifier sur une branche du dépôt.

La recette part de l’archive Gecko vérifiée et contrôle ses fichiers extraits.
`runtime-files.json` enregistre les empreintes du runtime assemblé. La signature
refuse un fichier modifié ou ajouté après cet assemblage, notamment un script
d’autoconfiguration étranger. `stage` copie la liste approuvée dans un dossier
neuf et distinct ; il ne retire aucun fichier d’un runtime utilisé localement.
Le dossier de staging doit être neuf pour chaque préparation. Si un antivirus y
ajoute des fichiers, le contrôle final refuse aussi la publication : utiliser
alors un runner de compilation propre. La clé de publication reste à configurer
ultérieurement ; ce contrôle n’en génère pas.

## GitHub Actions dans le dépôt du code

Le workflow **Prepare signed Windows release** se déclenche manuellement. Il
compile, exécute les tests natifs, teste la signature et le remplacement, puis
produit un artefact téléchargeable. Les versions de dépendances de compilation
et du moteur restent celles verrouillées dans le projet.

Configurer les valeurs suivantes dans **Settings > Secrets and variables >
Actions** du dépôt du code :

| Type | Nom | Valeur |
| --- | --- | --- |
| Variable | `CYURIUM_RELEASE_REPOSITORY` | `VOTRE_COMPTE/CyUrium-Releases` |
| Variable | `CYURIUM_RELEASE_PUBLIC_KEY` | Clé publique SPKI base64 |
| Secret | `CYURIUM_RELEASE_SIGNING_KEY` | Contenu PEM de la clé privée |
| Secret facultatif | `CYURIUM_RELEASE_TOKEN` | Accès `Contents: write` au dépôt public de destination |

Le jeton de publication est utilisé uniquement si l’option `create_draft` est
cochée. Le workflow prépare alors une **Release en brouillon**, avec ses trois
fichiers. La publication se fait ensuite depuis GitHub après relecture. Par
défaut, il produit seulement l’artefact signé, sans écrire dans un autre dépôt.
Une Release existante n’est pas écrasée. La clé privée est écrite dans le dossier
temporaire du runner pour la signature, puis supprimée ; elle n’entre ni dans
l’artefact ni dans le paquet du navigateur.

Le workflow signé exige le dépôt de destination, les variables et les secrets
indiqués ci-dessus. Il est indépendant de la publication manuelle de l’alpha.

`python tools/check_gecko.py` compare le verrou avec la version ESR publiée par
Mozilla, sans téléchargement de moteur ni installation. Le workflow refuse de
préparer une publication si ce contrôle signale du retard. Une mise à niveau du
verrou utilise des caches d’extraction séparés par empreinte d’archive. Toute
nouvelle version du moteur doit ensuite repasser les essais d’intégration ; le
contrôle de version seul ne prouve pas l’absence de vulnérabilité.

## Validation et références

CyUrium est désormais sous `GPL-3.0-only`. Les paquets contiennent `LICENSE`,
`NOTICE` et `THIRD_PARTY_NOTICES.md` ; leur présence est exigée lors de la
fabrication d’une Release. Avant toute publication publique des binaires,
mettre également à disposition les sources correspondant exactement à la
version distribuée, avec les scripts de compilation et les sources des
dépendances requises, conformément à leurs licences. Un dépôt de développement
privé reste possible si les destinataires peuvent obtenir ces sources par un
autre moyen, par exemple une archive jointe à la Release. Le dossier `dist/`
avec les seuls binaires ne constitue pas cette archive de sources.

Conserver les licences et avis amont : Gecko reste sous MPL et ses licences
associées, uBlock Origin sous sa GPL, les composants MIT sous MIT. Les profils,
historiques, jetons et clés privées ne font pas partie des sources à publier.
La publication de binaires conserve ces notices et les sources correspondantes.

```powershell
python -m unittest discover -s tests -p test_release.py
python tools/test_update_channel.py
python tools/test_updater.py
python tools/test_security.py
python tools/test_bridge_peer.py
```

Le test de transport utilise une API GitHub simulée en HTTP sur `127.0.0.1`,
uniquement avec un profil QA isolé et les deux marqueurs de test explicites.
Il vérifie aussi les redirections refusées, les signatures et fichiers altérés,
les ZIP trop grands et chemins sortant du staging, la nouvelle tentative après
un ZIP rejeté, les canaux, les limites d’API, l’absence de cookies et la
vérification automatique.
Le test ne modifie aucun magasin de certificats. Le téléchargement d’une vraie
Release du futur dépôt reste à vérifier après sa publication.

Contrats GitHub : [dernière Release](https://docs.github.com/en/rest/releases/releases#get-the-latest-release),
[téléchargement des assets et redirections](https://docs.github.com/en/rest/releases/assets#get-a-release-asset),
[limites de l’API](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api),
[secrets Actions](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-secrets),
[création de brouillons avec gh](https://cli.github.com/manual/gh_release_create).
