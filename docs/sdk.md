# MCP, SDK et conversation native

L’intégration Windows utilise **CyToolsCore 0.10.1 MIT**, son vrai Runtime,
ses contrats CyTools v1, ses jobs, sessions, identités et son adaptateur MCP
officiel 1.30.0. Le SDK ne fournit pas de bibliothèque C++ complète : un
Python privé accompagne l’application, tandis que les opérations du navigateur
s’exécutent dans le C++ existant et dans les acteurs Gecko. L’interface est
entièrement Dear ImGui. Aucun Python système ni projet voisin n’est nécessaire
dans la distribution.

## Conversation dans le navigateur

Le bouton **Conversation** de la barre d’outils ouvre le panneau latéral natif.
La séparation avec la page est redimensionnable ; chaque conversation possède
son brouillon, son historique et sa session fournisseur. Fermer le panneau laisse
un tour en cours continuer. Arrêter demande l’annulation ; fermer un onglet de
conversation termine sa propre session.

Dans **Réglages**, choisir un connecteur, tester la connexion, choisir un modèle
et enregistrer. Les CLI existantes utilisent leur authentification native.
Pour une API, saisir le **nom** d’une variable d’environnement contenant la clé,
puis relancer l’application après sa définition. Ne jamais coller une clé dans
un champ de modèle, un message ou une configuration. Aucun compte, CLI ou modèle
n’est téléchargé ni interrogé automatiquement à l’ouverture du panneau.

Les connecteurs portables existants sont repris sans modification depuis
**CyUEMCPSDK 0.1.8 MIT** : Codex App Server / CLI JSONL, Claude CLI,
Antigravity, OpenCode, Ollama, Claude API et OpenRouter. Les seuls descripteurs
sans transport implémenté ne sont pas proposés comme fonctionnels. Le moteur
GGUF autonome et la voix de CyTools_AIChat ne sont pas embarqués dans cette version.

Codex App Server reçoit les outils du navigateur via son protocole de fonctions
dynamiques expérimental. Lecture seule par défaut ; le contrôle s’active
explicitement dans le panneau ou les paramètres. Le service dérive l’identité
de la session authentifiée et soumet les appels au même Runtime CyToolsCore.
Un Codex installé qui ne supporte pas ce protocole peut retourner une erreur ;
aucun envoi payant n’est relancé automatiquement vers un autre transport.
Le scheduler Core limite les travaux fournisseurs à trois parmi quatre workers :
un appel d’outil navigateur imbriqué garde une place et ne bloque pas tous les tours.

Les autres connecteurs assurent la conversation. **Joindre la page active** lit
la page via le SDK, affiche la pièce jointe et l’envoie avec le prochain message.
Le contenu de page est marqué comme donnée non fiable. Il n’est jamais joint
automatiquement. Le fournisseur sélectionné reçoit le message et ses pièces jointes.

Les conversations et le réglage courant sont conservés dans
`<profil>/sdk/private/assistant/conversations.bin`, protégé par DPAPI sous Windows.
Après redémarrage, une nouvelle session fournisseur reçoit le transcript conservé ;
aucune ancienne session du compte n’est reprise implicitement. Les instantanés
d’interface sont bornés aux derniers caractères pour maintenir la réactivité.
Les métadonnées des jobs CyToolsCore résident séparément dans SQLite, dans le
répertoire privé au compte ; DPAPI ne chiffre pas tout le profil ni toute cette base.

## MCP externe

Dans **Paramètres > Système**, activer **le service local CyToolsCore**,
puis **Copier la configuration MCP**. L’application doit rester ouverte.
Le contrôle des onglets/pages est une autorisation distincte de l’accès en lecture.
Le panneau de conversation fonctionne sans activer ce service externe.

Exemple de configuration : remplacer les chemins par ceux de votre installation
et du profil utilisé par le navigateur.

```json
{
  "mcpServers": {
    "CyUrium": {
      "command": "C:\\Applications\\CyUrium\\CyUrium.exe",
      "args": ["--mcp", "--profile", "C:\\Applications\\CyUrium\\profile"]
    }
  }
}
```

`--mcp` est une entrée stdio sans interface graphique : elle se connecte à
l’instance explicitement activée, avec un client distinct par connexion.
Elle ne démarre pas le navigateur ni n’active ses permissions à sa place.
Le catalogue, les schémas et les erreurs sont ceux du SDK. Un appel `op_*`
retourne un job ; consulter `cy_get_job` jusqu’à Completed, Failed ou Cancelled.

## SDK applicatif

Le client Python livré dans `sdk/client.py` accepte le chemin du profil :

```python
from client import CyUriumClient

client = CyUriumClient(r"C:\Applications\CyUrium\profile")
session = client.call("CreateSession", temporary=True)["sessionId"]
job = client.call("SubmitJob", sessionId=session,
                  operationId="browser_state", parameters={})
status = client.call("GetJob", jobId=job["jobId"])
# Interroger jusqu’à la fin réelle du job, puis terminer la tâche.
client.call("FinishTask", sessionId=session, cancelRunning=True)
```

Utiliser le Python privé `sdk/python/python.exe -I -B` : ses chemins privés
incluent le client et le SDK. Le contrat HTTP CyTools v1 est également utilisable
depuis C++/C# ; aucune nouvelle variante du protocole n’est introduite.

## Opérations exposées

| Capacité réelle | Opération | Contrôle natif / contraintes |
|---|---|---|
| Fenêtres et onglets | `browser_state` | Fenêtres, IDs stables, sélection, URL, titre, chargement/TLS |
| Nouvelle fenêtre / fermeture | `window_new`, `window_close` | Vraies fenêtres ; dernière fenêtre protégée |
| Création, sélection, fermeture d’onglet | `tab_open`, `tab_select`, `tab_close` | Même BrowserWorkspace que l’interface |
| Réordonner, déplacer, détacher | `tab_move`, `tab_detach` | Document, historique et formulaires conservés |
| Navigation et historique | `tab_navigate`, `tab_history` | HTTP(S) ; back/forward/reload/stop réels |
| Lecture de page | `page_snapshot` | Texte, titre, sélecteurs et documentId ; sans valeurs de champs |
| Clic, remplissage, défilement | `page_click`, `page_fill`, `page_scroll` | URL et documentId vérifiés ; champs sensibles exclus |
| Fournisseurs / conversations | `assistant_probe/open/send/close` | Jobs Core ; permission `assistant.use` réservée à l’interface native |

Les frames, captures d’écran, JavaScript arbitraire, shell, fichiers locaux,
installation d’extensions, décisions de permissions et mises à jour ne sont pas
exposés aux clients MCP de cette première intégration. Les téléchargements causés
par une page continuent à passer par la gestion habituelle de Gecko. Les contrôles
génériques ImportFile / ExportJob du SDK sont refusés dans ce produit.

## Sécurité et distribution

- Service externe désactivé par défaut ; lorsqu’il est actif, écoute uniquement
  sur `127.0.0.1`, port éphémère. Origines web refusées, Bearer requis,
  redirections et proxies exclus par le client local.
- Secret de découverte par lancement, protégé DPAPI ; jeton distinct par client.
  Chaque client ne lit/annule que ses propres jobs. Désactiver le service arrête
  le compagnon et révoque ses connexions. Un changement d’autorisation le redémarre.
  Un arrêt forcé peut laisser un ancien fichier de découverte chiffré ; son port et
  ses jetons ne permettent plus d’atteindre le navigateur. Le lancement suivant le remplace.
- Le service communique avec le navigateur par les pipes possédés de son processus,
  sans partager le canal privilégié Gecko. Le C++ revalide les opérations et
  applique toutes les mutations entre les frames ImGui. Un Job Object Windows
  arrête le compagnon et ses descendants avec le navigateur.
- Les clients autorisés partagent le profil de navigation et peuvent consulter
  tous ses onglets. Il n’y a pas encore d’autorisation par onglet ou par origine.
  Une permission de contrôle permet des actions avec effets réels sur les sites.
- Les demandes natives de caméra, microphone, fichiers, etc. gardent leurs flux
  existants. Le MCP ne leur répond pas automatiquement.
- Un appel de page bloqué conserve la réservation du job jusqu’à sa réponse ou
  l’arrêt effectif du processus ; l’annulation ne prétend pas annuler un clic déjà exécuté.

`sdk/runtime.lock.json`, `requirements.lock.json` et `providers.lock.json` épinglent
les archives, roues et sources réellement livrées. `tools/prepare_sdk.py` vérifie
les SHA-256 et inclut les licences complètes dans `LICENCES/SDK` et `LICENCES/`.
Le wheel Core MIT est embarqué dans `sdk/packages`, sans dépendre du dépôt privé
de développement. Python 3.14.10 embarqué vient de python.org ; les dépendances
sont installées dans l’application, sans pip ni site utilisateur à l’exécution.

Validation : `tools/test_sdk.py` utilise le vrai Gecko et un vrai client MCP ;
`tools/test_assistant.py` vérifie les commandes du panneau natif et le transport
Ollama existant face à un serveur local de test, sans compte ni modèle téléchargé.
L’annulation, l’isolation des conversations et l’extension Codex sont aussi testées
avec des échanges simulés du protocole. Aucun compte Codex/Claude/OpenRouter réel
n’a été appelé pour cette livraison. macOS/Linux restent non validés pour le
runtime privé et l’hébergement Gecko ; aucune version Android n’est modifiée.

Références : [CyToolsCore](https://github.com/MrMybal/CyToolsCore),
[fournisseurs CyUEMCPSDK](https://github.com/MrMybal/CyUEMCPSDK),
[Codex App Server et fonctions dynamiques](https://learn.chatgpt.com/docs/app-server).
