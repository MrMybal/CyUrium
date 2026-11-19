# MCP, CLI et conversation native

Guide de **CyUrium 0.4.0 Alpha 1**, disponible dans les [Releases GitHub](https://github.com/MrMybal/CyUrium/releases/tag/v0.4.0-alpha.1).
**CyMCPSDK 0.7.0**, révision `791d4d7d7680213b282ec8529d248a10f8494d77`, est compilé en C++ depuis les sources locales de CyMCPSDK. Le snapshot inchangé et ses empreintes sont dans `third_party/cymcpsdk/` et `sdk/cymcpsdk.lock.json` ; aucun dépôt voisin n’est nécessaire pour reconstruire.

Modules liés : **Core, Platform, Http, MCP, AI**. Local, ChatUI et ChatApp sont désactivés. Le snapshot CyUI du navigateur ne contient pas DocumentViews, requis par ChatUI : le panneau Dear ImGui utilise **Conversation**. `CyUrium.exe` et `CyUriumCLI.exe` n’embarquent plus Python. Python reste un outil de compilation et de tests pour le développeur.

## Registre et autorisations

Le catalogue métier est dans `sdk/operations.json`. Le même **Registry** sert au panneau, au CLI et au serveur MCP. Les handlers passent par les pipes du compagnon possédé et exécutent les mutations sur le thread UI existant ; les opérations DOM utilisent les acteurs Gecko.

L’accès externe est désactivé par défaut. Dans **Paramètres > Système**, activer CyMCPSDK, puis séparément le contrôle du navigateur si nécessaire. Le panneau Conversation fonctionne sans accès externe. Les autorisations natives de shell/fichiers des agents restent refusées : le contrôle des onglets ne leur accorde pas de permission système supplémentaire.

| Opérations partagées | Comportement réel |
| --- | --- |
| `browser_state`, `window_new/close` | Fenêtres, sélection, onglets ; fermeture de la dernière fenêtre refusée |
| `tab_open/close/select/move/detach` | Modèle et fenêtres natifs ; identité du document conservée |
| `tab_navigate`, `tab_history` | HTTP(S), retour, avance, rechargement, arrêt |
| `page_snapshot`, `page_capture` | Capture explicite du document principal, sans valeurs de champs ; `page_capture` retourne une référence privée |
| `page_click/fill/scroll` | URL/documentId vérifiés ; contrôles sensibles et protocoles privilégiés refusés |
| `bookmarks_list/add/remove`, `history_list` | Vrais favoris enregistrés et adresses saisies ; filtre littéral et pagination |
| `settings_get/set`, `profile_info` | Lecture ; modification limitée au thème, à l’échelle et à la barre de favoris |
| `downloads_list`, `permissions_list`, `extensions_list`, `graphics_list` | Instantané paginé des services natifs |
| `internal_page_open` | Nouvel onglet Paramètres/Favoris/Historique/Téléchargements/Extensions |
| `resources_capture/read/grep` | Références privées, lecture par morceaux et grep littéral des textes exposés |
| `jobs_submit/list/get/cancel/delete` | Jobs métier durables, isolés par identité cliente |
| `assistant_probe/open/send/close` | Conversation : interface native, ou accès externe explicitement autorisé |

`list`/`describe` donnent les schémas complets réellement exécutés. Les clients externes découvrent le catalogue, mais chaque appel est autorisé séparément. `settings_set` ne peut pas changer les droits SDK. Les outils distants sélectionnés sont réservés aux conversations autorisées.

JavaScript arbitraire, shell, parcours de fichiers, installation/suppression d’extensions, octroi de permissions, exécution d’un téléchargement et installation d’une mise à jour restent dans l’interface : ces parcours requièrent les dialogues existants. Pause/reprise des téléchargements, modifications de règles et gestion des extensions restent dans leurs onglets ; leur lecture est exposée. Aucun script n’approuve automatiquement une confirmation. Une annulation ne défait pas les effets déjà appliqués.

## Ligne de commande

Le compagnon console est livré à côté de CyUrium. Exemple PowerShell :

```powershell
& '.\dist\windows\CyUrium\CyUriumCLI.exe' --help
& '.\dist\windows\CyUrium\CyUriumCLI.exe' list
& '.\dist\windows\CyUrium\CyUriumCLI.exe' describe bookmarks_add
& '.\dist\windows\CyUrium\CyUriumCLI.exe' invoke browser_state
& '.\dist\windows\CyUrium\CyUriumCLI.exe' invoke bookmarks_add --args-file bookmark.json
```

`bookmark.json` :

```json
{"title":"Documentation","url":"https://example.org/documentation"}
```

`--args JSON` est aussi accepté ; `--args-file` évite les différences de quoting des shells. `--profile DIR` ou `--instance DIR` choisit l’instance par son dossier de profil, qui doit être celui du navigateur. Par défaut Windows : `%APPDATA%\CyUrium\CyUrium`. `--client mon-script` conserve une identité et ses jobs après redémarrage. Son credential est généré et protégé par le compte système, jamais passé dans la ligne de commande.

```powershell
& '.\dist\windows\CyUrium\CyUriumCLI.exe' --profile 'D:\Profiles\Work' --client script jobs submit browser_state
& '.\dist\windows\CyUrium\CyUriumCLI.exe' --profile 'D:\Profiles\Work' --client script jobs get ID
& '.\dist\windows\CyUrium\CyUriumCLI.exe' --profile 'D:\Profiles\Work' --client script jobs cancel ID
```

Sans interface, `--offline` lit `bookmarks_list`, `history_list`, `settings_get` et `profile_info` dans le profil enregistré, si son accès externe est activé. Aucune interface ne démarre. `list`/`describe` sans instance donnent le catalogue métier statique ; avec instance, ils incluent les opérations de service et les outils distants réellement connectés.

Succès : JSON sur stdout. Échec : JSON sur stderr, stdout vide. Codes : **0** succès, **2** arguments/schéma/opération inconnue, **3** instance/connexion indisponible, **4** accès refusé, **5** autre erreur, **6** annulation. Toutes les anciennes options de lancement de CyUrium sont conservées.

## Connexion MCP externe

Copier la configuration depuis les Paramètres, ou utiliser :

```json
{
  "mcpServers": {
    "CyUrium": {
      "command":"C:\\Applications\\CyUrium\\CyUriumCLI.exe",
      "args":["--profile","D:\\Profiles\\Work","--client","external-ai","mcp"]
    }
  }
}
```

`CyUrium.exe --mcp --profile DIR` est conservé. Ces entrées sont des ponts stdio sans fenêtre vers le serveur MCP natif de l’instance choisie. Aucun token n’est nécessaire dans la configuration : découverte DPAPI et appairage délivrent un endpoint et un Bearer propres à chaque identité, uniquement sur `127.0.0.1`, port éphémère. Le serveur refuse les origines de pages et les Host non locaux. Désactiver le service ou changer ses droits ferme les processus et connexions.

La révision utilisée fournit MCP **2026-07-28**, découverte sans session et abonnements SSE, ainsi que la négociation historique **2025-11-25** et **2025-06-18**. Le pont stdio conserve métadonnées, sessions historiques, notifications et annulations. Un fichier de découverte périmé après arrêt forcé produit une erreur de connexion, sans redémarrer le navigateur ni une action.

## Conversation et fournisseurs

Ouvrir **Conversation > Réglages**, choisir le fournisseur, tester sa connexion et choisir un modèle détecté. La détection est explicite ; aucun modèle, compte ou outil n’est installé automatiquement. Le panneau affiche les capacités testées : outils, flux et reprise. Les CLI utilisent leur authentification ; les API lisent une clé depuis le **nom d’une variable d’environnement**, jamais un secret enregistré dans les préférences.

Le catalogue natif inclut notamment Codex App Server/Exec, Claude Code, OpenCode, Antigravity, les transports ACP, Ollama, OpenAI, Anthropic, OpenRouter et les API locales compatibles. Les anciens identifiants Claude CLI/Claude et Codex CLI JSONL sont traduits. Un transport qui ne fournit pas d’outils ne gagne pas cette capacité par le panneau ; choisir un fournisseur qui les annonce. Aucun envoi n’est réessayé implicitement sur un autre transport.

Réponses progressives, annulation, brouillons et pièce jointe explicite de la page active sont intégrés. Pages et résultats distants sont des données non fiables. L’historique complet et le checkpoint natif résident dans `sdk/private/assistant/conversations.bin`, chiffré DPAPI sous Windows. L’interface charge dix messages à la fois avec Précédents/Suivants ; le checkpoint conserve les groupes d’outils et leurs résultats. Les agents reprennent leur session exacte si le fournisseur le permet. Un tour inachevé après crash est marqué interrompu et n’est pas renvoyé.

## Optimisations indépendantes

Dans Réglages, ouvrir **Optimisations et outils distants**. Enregistrer puis **Relancer le service** pour appliquer les changements. Options supplémentaires : mêmes clés dans le fichier protégé `sdk/options.bin`.

| Option | Défaut | Effet |
| --- | --- | --- |
| `compact_tools` / `discovery_threshold` | true / 16 | Cinq outils de recherche, schéma, appel, lecture et grep pour le chat ; catalogue externe complet |
| `inline_bytes` / `chunk_bytes` | 16384 / 32768 | Seuil de référence et morceaux UTF-8 |
| `resource_bytes` / `result_ttl_ms` | 64 Mio / 300000 ms | Quota mémoire et rétention des résultats privés |
| `disk_spool` / `disk_quota_bytes` | false / 256 Mio | Production progressive privée dans `sdk/private/results` ; seuil SDK 1 Mio ou quota mémoire inférieur |
| `client_cache` / `cache_ttl_ms` | true / 3000 ms | Catalogues/lectures qui annoncent un TTL ; aucune mutation cachée |
| `notifications` | true | Journal de changements et abonnements compatibles ; TTL/rafraîchissement conservés |
| `performance` | false | JSONL privé : durées, volumes, statuts et cache ; aucun message/résultat/token/identifiant de session |
| `context_bytes` / `context_recent_turns` | 0 / 8 | Fenêtre API hôte ; 0 = complète ; système, tour actif et groupes outils conservés |
| `external_assistant` | false | Autoriser explicitement les clients appairés à utiliser leurs propres conversations |
| `durable_jobs` | 128 | Résultats durables jusqu’à suppression explicite |

Les agents CLI conservent leur contexte natif. Aucun résumé automatique n’est ajouté. Un budget API insuffisant pour le système/tour actif produit une erreur explicite ; l’historique intégral reste dans le profil. Le grep porte sur les captures exposées, sans ripgrep ni parcours de fichiers. Les grandes réponses fournissent `uri`, puis un manifeste `firstUri` et des morceaux avec `nextUri`. Une référence expirée échoue sans rejouer l’opération. Les dernières captures publiques et métadonnées sont remplacées explicitement ; les résultats retenus sont privés à leur client.

## Outils d’autres applications

Le tableau `remote_servers` est éditable dans le panneau. Le serveur et chaque outil sont sélectionnés explicitement :

```json
[
  {
    "id":"editor",
    "enabled":true,
    "url":"http://127.0.0.1:9000/mcp",
    "token_env":"EDITOR_MCP_TOKEN",
    "allowed_tools":["scene_list"],
    "allowed_resources":["editor://selection"]
  }
]
```

En stdio, utiliser `executable` et `arguments` à la place de `url` ; le processus démarre dans un dossier privé. HTTP exige HTTPS ou loopback. Les secrets restent dans l’environnement. McpClient découvre et pagine les catalogues, expose seulement les outils choisis sous `remote_<id>_<empreinte>` et les URI sélectionnées. Une erreur distante reste une erreur exploitable par le modèle. Les abonnements invalident les caches ; les transports historiques utilisent TTL et **Rafraîchir les serveurs connectés**. Une nouvelle sélection requiert un redémarrage du service ; un résultat distant ne peut pas ajouter de permission.

## Migration et jobs

L’ancien fichier DPAPI de conversation est lu sans l’effacer. Les historiques API sont convertis lors d’un envoi explicite. Si un ancien agent CLI n’a pas enregistré son identifiant de session, le panneau garde le transcript et propose **Reprendre dans une nouvelle session** ; aucune session du compte n’est choisie silencieusement. Préférences, favoris, pages et options de lancement conservent leurs formats.

Les nouveaux jobs résident dans `sdk/private/jobs-v2/*.bin` : propriétaire, état et résultat complet protégés. Les captures soumises comme jobs gardent le contenu complet, pas une référence éphémère. Les résultats survivent au redémarrage et à l’expiration SDK jusqu’à `jobs delete`. Queued/Running deviennent Interrupted après un arrêt, sans rejeu. L’annulation est coopérative et ne défait pas un clic déjà exécuté.

Importer les anciens jobs avec leur jeton d’origine dans une variable d’environnement : `--legacy-token-env NOM jobs list`. Le lecteur SQLite système Windows ne lit que les jobs du propriétaire, depuis une copie de la base et de son WAL ; la base originale reste inchangée. L’import est idempotent. Sans credential, les anciens résultats restent dans leur base et ne sont pas réattribués. Les wrappers Python et le protocole CyTools v1 sont remplacés par ces entrées natives ; les anciens scripts clients doivent migrer.

## Construction et validation locale

```powershell
python tools/build.py
python tools/test_sdk.py
python tools/test_assistant.py
python tools/test_web.py
python -m unittest discover -s tests -p test_windows_package.py
```

Profils QA isolés : vrai navigateur, favoris enregistrés, capture Gecko, CLI/MCP lecture/mutation/refus, jobs durables/import propriétaire, outil distant d’une seconde instance depuis le chat, erreur, annulation, chunks UTF-8, cache/notifications et contexte réduit sans perte d’historique. Seul le fournisseur de génération est simulé localement ; aucun compte payant n’est utilisé. Les tests sont locaux.

Limites : Windows x64 validé ; macOS/Linux restent à valider avec leurs moteurs web. Aucun moteur d’inférence, voix ou téléchargement automatique de modèles. Un enregistrement privé est limité à 16 Mio en lecture ; le journal des conversations n’est pas un stockage d’exports illimités. Les captures DOM se limitent au document principal et au nombre de caractères demandé.
