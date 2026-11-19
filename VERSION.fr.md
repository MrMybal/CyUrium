# CyUrium 0.4.0 — Alpha 1

[README](README.fr.md) · [English](VERSION.md) · [Suivi des problèmes](https://github.com/MrMybal/CyUrium/issues)

## Disponibilité

**0.4.0 Alpha 1** est un navigateur Windows x64 expérimental, destiné aux essais et aux retours utilisateurs.
La [Release GitHub](https://github.com/MrMybal/CyUrium/releases/tag/v0.4.0-alpha.1)
propose un installateur par utilisateur, un ZIP portable, les sources correspondantes et les empreintes SHA-256.
macOS et Linux conservent leurs cibles d’interface de bureau ; l’intégration Gecko native et l’exécution du SDK y restent à valider.
Android et Meta Quest relèvent de CyVR_Urium.

## Changements depuis 0.3.0

- **CyMCPSDK 0.7.0 natif** remplace l’intégration CyToolsCore et son runtime Python.
- `CyUriumCLI.exe` liste les opérations, décrit les schémas et pilote le vrai navigateur en JSON, avec les lectures compatibles du profil hors interface.
- Chat, CLI et MCP externe utilisent un registre commun et les droits lecture/contrôle du navigateur. L’entrée existante `CyUrium.exe --mcp` est conservée.
- Les conversations natives proposent réponses progressives, annulation, pièces jointes explicites, détection des fournisseurs/modèles et restauration des checkpoints. Historiques et longs messages sont paginés sans tronquer le transcript enregistré.
- Les outils et ressources MCP distants sélectionnés explicitement sont utilisables dans le chat. Cache, notifications, découverte compacte, résultats par morceaux, disque temporaire, mesures techniques et fenêtre de contexte API sont réglables séparément.
- Les jobs durables conservent leur résultat complet après redémarrage et expiration des références. Les anciens jobs s’importent avec le credential d’origine de leur propriétaire ; les actions interrompues ne sont pas rejouées.
- Les grands messages interface/compagnon sont écrits progressivement sur le pipe non bloquant. L’appairage de CLI concurrents conserve une identité cliente commune.
- Le moteur embarqué est mis à jour vers **Gecko ESR 153.7.0**, depuis la distribution officielle Mozilla.

Favoris, préférences, onglets déplaçables, fenêtres indépendantes, permissions,
téléchargements, extensions et uBlock Origin intégré restent disponibles.

## Composants verrouillés

| Composant | Version | Rôle |
| --- | --- | --- |
| CyUrium | 0.4.0 · Alpha 1 | Navigateur natif et compagnon console. |
| Gecko | 153.7.0 ESR | Moteur web Windows. |
| Dear ImGui / SDL | 1.92.5-docking / 3.2.28 | Interface et rendu GPU. |
| uBlock Origin | 1.75.0 | Extension embarquée signée par Mozilla. |
| CyMCPSDK | 0.7.0, révision `791d4d7d7680213b282ec8529d248a10f8494d77` | Core, Platform, Http, MCP et AI compilés depuis les sources. |

Les sources et archives exactes sont verrouillées dans `sdk/cymcpsdk.lock.json`,
`engine/runtime.lock.json` et la configuration des dépendances natives.
Le navigateur ne demande plus de runtime Python ; Python reste un outil de compilation et de tests.

## Limites documentées

- **Plateformes :** Windows x64 est la distribution navigateur validée. Le placement des onglets Linux demande X11/XWayland ; le placement Wayland natif n’est pas pris en charge.
- **Extensions :** certaines API dépendantes de l’interface Firefox, les barres latérales, menus contextuels et créations d’onglets/fenêtres demandent encore des adaptations ; toutes les extensions Firefox ne sont pas compatibles.
- **Web :** capture de périphériques, sites variés, plein écran HTML/vidéo et accessibilité demandent une validation plus large.
- **Fournisseurs IA :** des CLI/comptes installés ou des API/services locaux configurés sont nécessaires. Outils, flux et reprise native dépendent du transport choisi. Les scénarios de protocole n’utilisent pas de compte fournisseur réel ; aucun modèle ni CLI n’est installé automatiquement.
- **Sessions :** la restauration des formulaires est facultative et désactivée par défaut sous Windows. Mot de passe/fichier/champs masqués et champs reconnus de paiement/code à usage unique sont exclus. DPAPI protège les enregistrements de session/conversation pour le compte ; il ne chiffre pas tout le profil Gecko ni les préférences.
- **Récupération :** jobs et tours interrompus sont enregistrés sans rejeu. Une annulation ne défait pas une action déjà appliquée. Les redémarrages automatiques rapprochés du moteur sont limités à trois.
- **Volumes :** un enregistrement privé est limité à 16 Mio ; les captures portent sur le document principal et le nombre de caractères demandé. Les références SDK expirent selon leur rétention ; les résultats durables restent jusqu’à suppression.
- **Mises à jour et installateur :** les alphas se mettent à jour par installateur/ZIP manuel. Authenticode et le canal de mise à jour signé restent à configurer. La désinstallation conserve le profil.
- **Performances :** les mesures de l’interface excluent les processus Gecko et la mémoire GPU ; aucun benchmark de navigation multiplateforme n’est revendiqué.

## Migration et retours

Les formats des favoris et réglages sont conservés. Les anciennes conversations sont
préservées ; sans identifiant enregistré de session CLI native, la reprise exige une
nouvelle session explicite. Les anciens clients HTTP/Python doivent adopter le CLI/MCP natif.
Voir le [guide SDK et migration](docs/sdk.md).

Signaler les problèmes reproductibles dans le [suivi du projet](https://github.com/MrMybal/CyUrium/issues),
avec plateforme, étapes et résultat attendu. Ces limites décrivent le support actuel,
sans promettre l’absence d’autres bugs.

[Ancienne Release 0.3.0 Alpha 1](https://github.com/MrMybal/CyUrium/releases/tag/v0.3.0-alpha.1)
· [Architecture](docs/architecture.md) · [Préparer une Release](docs/releases.md)
