// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_ui.hpp"
#include <algorithm>
#include <cstdio>

namespace cyurium {
void BrowserUi::assistant_panel(float x,float top,float width,float height){
    using Json=nlohmann::json;
    auto& s=model_->profile->services;auto& state=s.assistant;
    auto command=[&](std::string action,Json args=Json::object()){
        args["action"]=std::move(action);if(!args.contains("conversationId"))args["conversationId"]=assistant_conversation_;s.assistant_commands.push_back(std::move(args));
    };
    ImGui::SetCursorScreenPos({origin_.x+x*scale_,origin_.y+top*scale_});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{8*scale_,5*scale_});
    ImGui::BeginChild("assistant-panel",{width*scale_,height*scale_},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
    ImGui::TextUnformatted("Conversation");ImGui::SameLine();
    if(ImGui::SmallButton(assistant_settings_?"Retour":"Réglages"))assistant_settings_=!assistant_settings_;
    ImGui::SameLine();if(ImGui::SmallButton("Fermer"))s.assistant_visible=false;
    ImGui::Separator();
    if(state.empty()){
        ImGui::TextWrapped("Démarrage de CyMCPSDK…");
        const auto service=s.state.value("sdk",Json::object());ImGui::TextWrapped("%s",service.value("message","").c_str());ImGui::EndChild();ImGui::PopStyleVar();return;
    }
    if(!assistant_loaded_&&state.contains("settings")){
        const auto cfg=state["settings"];assistant_connector_=cfg.value("connector","codex");assistant_transport_=cfg.value("transport","app-server");
        std::snprintf(assistant_model_.data(),assistant_model_.size(),"%s",cfg.value("model","").c_str());
        std::snprintf(assistant_endpoint_.data(),assistant_endpoint_.size(),"%s",cfg.value("endpoint","").c_str());
        std::snprintf(assistant_executable_.data(),assistant_executable_.size(),"%s",cfg.value("executable","").c_str());
        std::snprintf(assistant_key_env_.data(),assistant_key_env_.size(),"%s",cfg.value("api_key_env","").c_str());assistant_loaded_=true;
    }
    auto settings=[&](){return Json{{"connector",assistant_connector_},{"transport",assistant_transport_},{"model",assistant_model_.data()},
        {"endpoint",assistant_endpoint_.data()},{"api_key_env",assistant_key_env_.data()},{"executable",assistant_executable_.data()}};};
    if(assistant_settings_){
        ImGui::TextWrapped("CyMCPSDK 0.7.0 · fournisseurs natifs");
        ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##connector",assistant_connector_.c_str())){
            for(auto& c:state.value("connectors",Json::array()))if(ImGui::Selectable(c.value("name","").c_str(),assistant_connector_==c.value("id",""))){
                assistant_connector_=c.value("id","");assistant_transport_=c.value("transport","");assistant_model_[0]=assistant_executable_[0]=0;
                const auto defaults=c.value("defaults",Json::object());std::snprintf(assistant_endpoint_.data(),assistant_endpoint_.size(),"%s",defaults.value("base_url","").c_str());std::snprintf(assistant_key_env_.data(),assistant_key_env_.size(),"%s",defaults.value("credential_env","").c_str());
            }ImGui::EndCombo();
        }
        auto input=[&](const char* label,auto& buffer){ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);ImGui::InputText((std::string("##")+label).c_str(),buffer.data(),buffer.size());editing_|=ImGui::IsItemActive();};
        input("Modèle (vide : défaut du fournisseur)",assistant_model_);
        input("Adresse du serveur (HTTP local / HTTPS)",assistant_endpoint_);
        input("Nom de variable de la clé API",assistant_key_env_);
        ImGui::TextWrapped("Utilisez le nom de variable d’environnement, jamais la clé. Les CLI réutilisent leur connexion native.");
        if(ImGui::TreeNode("Avancé")){
            input("Exécutable CLI (facultatif)",assistant_executable_);
            if(assistant_connector_=="codex"){
                bool app=assistant_transport_!="cli-jsonl";
                if(ImGui::RadioButton("App Server · outils navigateur",app))assistant_transport_="app-server";
                if(ImGui::RadioButton("CLI JSONL · conversation",!app))assistant_transport_="cli-jsonl";
            }ImGui::TreePop();
        }
        ImGui::BeginDisabled(state.value("probeBusy",false));
        if(ImGui::Button("Tester la connexion"))command("probe",{{"settings",settings()}});ImGui::SameLine();
        if(ImGui::Button("Enregistrer")){command("configure",{{"settings",settings()}});assistant_settings_=false;}
        ImGui::EndDisabled();
        if(ImGui::Button("Détecter les fournisseurs installés"))command("discover");
        for(const auto& connection:state.value("discovered",Json::array()))ImGui::TextWrapped("%s : %s",connection.value("name","").c_str(),connection.value("state","").c_str());
        const auto probe=state.value("probe",Json::object());
        if(state.value("probeBusy",false))ImGui::TextUnformatted("Vérification en cours…");
        ImGui::TextWrapped("%s",probe.value("status","").c_str());ImGui::TextWrapped("%s",probe.value("message","").c_str());
        if(probe.contains("models")&&ImGui::BeginCombo("Modèles détectés",assistant_model_.data())){
            for(auto& item:probe["models"]){auto id=item.value("model",item.value("id",""));if(ImGui::Selectable(id.c_str()))std::snprintf(assistant_model_.data(),assistant_model_.size(),"%s",id.c_str());}ImGui::EndCombo();
        }
        if(probe.contains("capabilities")){const auto cap=probe["capabilities"];ImGui::TextWrapped("Connexion testée · outils : %s · flux : %s · reprise : %s",cap.value("tools",false)?"oui":"non",cap.value("streaming",false)?"oui":"non",cap.value("resume",false)?"oui":"non");}
        if(ImGui::TreeNode("Optimisations et outils distants")){
            auto revision=state.value("optionsRevision","");if(assistant_options_revision_!=revision){assistant_options_revision_=revision;assistant_options_value_=state.value("options",Json::object());std::snprintf(assistant_remote_config_.data(),assistant_remote_config_.size(),"%s",assistant_options_value_.value("remote_servers",Json::array()).dump(2).c_str());}
            auto toggle=[&](const char* label,const char* key,bool fallback){bool value=assistant_options_value_.value(key,fallback);if(ImGui::Checkbox(label,&value))assistant_options_value_[key]=value;};
            toggle("Découverte compacte des outils","compact_tools",true);
            toggle("Cache des lectures MCP","client_cache",true);
            toggle("Notifications de changements","notifications",true);
            toggle("Stockage temporaire privé sur disque","disk_spool",false);
            toggle("Mesures techniques sans messages ni secrets","performance",false);
            toggle("Autoriser les clients externes à utiliser l’IA","external_assistant",false);
            auto integer=[&](const char* label,const char* key,int fallback,int low,int high,int unit=1){int value=int(assistant_options_value_.value(key,std::size_t(fallback*unit))/unit);if(ImGui::InputInt(label,&value))assistant_options_value_[key]=std::size_t(std::clamp(value,low,high))*unit;editing_|=ImGui::IsItemActive();};
            integer("Fenêtre API (Kio, 0 = complète)","context_bytes",0,0,262144,1024);
            integer("Tours récents API","context_recent_turns",8,1,100);
            integer("TTL cache (ms)","cache_ttl_ms",3000,0,60000);
            integer("Rétention des résultats (s)","result_ttl_ms",300,1,86400,1000);
            integer("Quota mémoire résultats (Mio)","resource_bytes",64,1,1024,1024*1024);
            integer("Quota disque temporaire (Mio)","disk_quota_bytes",256,1,1024,1024*1024);
            ImGui::TextWrapped("L’historique complet reste conservé. Les agents CLI gèrent leur contexte natif. La recherche porte sur les captures demandées ; aucun dossier n’est ouvert à ripgrep.");
            ImGui::TextUnformatted("Serveurs MCP autorisés (JSON ; secrets par nom de variable)");
            ImGui::InputTextMultiline("##remote-mcp",assistant_remote_config_.data(),assistant_remote_config_.size(),{-1,110*scale_});editing_|=ImGui::IsItemActive();
            if(ImGui::Button("Enregistrer les options")){auto remotes=Json::parse(assistant_remote_config_.data(),nullptr,false);if(remotes.is_array()){assistant_options_value_["remote_servers"]=remotes;command("options",{{"options",assistant_options_value_}});}else s.notice="La liste de serveurs MCP doit être un tableau JSON.";}
            ImGui::SameLine();if(ImGui::Button("Relancer le service"))command("restart");
            if(ImGui::Button("Rafraîchir les serveurs connectés"))command("refresh-remotes");
            for(const auto& remote:state.value("remoteStatus",Json::array()))ImGui::TextWrapped("%s · cache : %llu accès · %llu lectures · %zu changements",remote.value("id","").c_str(),remote.value("cacheHits",0ULL),remote.value("cacheMisses",0ULL),remote.value("receivedChanges",std::size_t(0)));
            ImGui::TextWrapped("%s",state.value("notice","").c_str());ImGui::TreePop();
        }
        ImGui::Spacing();
        if(ImGui::Checkbox("Autoriser les actions sur le navigateur",&s.sdk_control))actions_.save=true;
        ImGui::TextWrapped("Lecture seule par défaut. Les fournisseurs compatibles avec les appels d’outils utilisent le registre du navigateur. Les capacités effectives sont affichées après le test de connexion. Un changement d’autorisation relance le service.");
        ImGui::TextWrapped("Aucun modèle ni CLI n’est installé automatiquement. Les messages et les pages jointes sont envoyés au fournisseur choisi.");
    }else{
        auto conversations=state.value("conversations",Json::array());
        if(!conversations.empty()&&std::none_of(conversations.begin(),conversations.end(),[&](auto& c){return c.value("id","")==assistant_conversation_;}))assistant_conversation_=conversations.back().value("id","");
        if(ImGui::BeginTabBar("conversations",ImGuiTabBarFlags_FittingPolicyScroll|ImGuiTabBarFlags_AutoSelectNewTabs)){
            for(auto& c:conversations){bool open=true;auto id=c.value("id","");auto label=c.value("title","Conversation")+"###"+id;
                if(ImGui::BeginTabItem(label.c_str(),&open)){assistant_conversation_=id;ImGui::EndTabItem();}
                if(!open)command("close",{{"conversationId",id}});
            }
            if(ImGui::TabItemButton("+",ImGuiTabItemFlags_Trailing)){command("configure",{{"settings",settings()}});command("new");}
            ImGui::EndTabBar();
        }
        if(assistant_last_conversation_!=assistant_conversation_){
            if(!assistant_conversation_.empty())command("history");
            assistant_drafts_[assistant_last_conversation_]=assistant_draft_.data();
            std::snprintf(assistant_draft_.data(),assistant_draft_.size(),"%s",assistant_drafts_[assistant_conversation_].c_str());assistant_last_conversation_=assistant_conversation_;
        }
        auto selected=std::find_if(conversations.begin(),conversations.end(),[&](auto& c){return c.value("id","")==assistant_conversation_;});
        const bool has=selected!=conversations.end(),busy=has&&selected->value("busy",false);
        if(has){auto cfg=selected->value("settings",Json::object());ImGui::TextWrapped("%s · %s · %s",cfg.value("connector","").c_str(),cfg.value("transport","").c_str(),selected->value("model",cfg.value("model","Défaut fournisseur")).c_str());}
        ImGui::TextUnformatted(s.sdk_control?"Accès navigateur : contrôle autorisé":"Accès navigateur : lecture seule");
        const auto attachment=state.value("attachment",Json());
        const float footer=attachment.is_object()?190.f:155.f;
        ImGui::BeginChild("assistant-messages",{0,std::max(50.f,ImGui::GetContentRegionAvail().y-footer*scale_)},ImGuiChildFlags_None);
        if(has){
            const auto offset=selected->value("historyOffset",std::size_t(0)),count=selected->value("historyCount",std::size_t(0));
            if(offset&&ImGui::SmallButton("Précédents"))command("history",{{"offset",offset>10?offset-10:0}});
            if(offset){ImGui::SameLine();}
            ImGui::TextDisabled("Messages %zu–%zu / %zu",count?offset+1:0,std::min(count,offset+10),count);
            if(offset+10<count){if(ImGui::SmallButton("Suivants"))command("history",{{"offset",offset+10}});ImGui::SameLine();if(ImGui::SmallButton("Derniers messages"))command("history");}
            int message_index=0;
            for(auto& message:selected->value("messages",Json::array())){
                auto text=message.value("text","");ImGui::TextUnformatted(message.value("role","")=="user"?"Vous":"Assistant");
                ImGui::TextWrapped("%s",text.c_str());
                ImGui::PushID(message_index);
                if(message.value("textOffset",std::size_t(0))&&ImGui::SmallButton("Début du message"))command("message-read",{{"index",message["messageIndex"]},{"offset",0}});
                if(message.contains("nextTextOffset")&&ImGui::SmallButton("Suite du message"))command("message-read",{{"index",message["messageIndex"]},{"offset",message["nextTextOffset"]}});
                ImGui::PopID();
                if(message.contains("attachment"))ImGui::TextDisabled("Contexte joint conservé");
                if(message.value("interrupted",false))ImGui::TextDisabled("Réponse interrompue");
                if(ImGui::BeginPopupContextItem(("message-"+std::to_string(message_index++)).c_str())){if(ImGui::MenuItem("Copier le texte affiché"))ImGui::SetClipboardText(text.c_str());ImGui::EndPopup();}
                ImGui::Spacing();ImGui::Separator();
            }
            if(!selected->value("toolActivity","").empty())ImGui::TextWrapped("%s",selected->value("toolActivity","").c_str());
            if(busy){ImGui::TextUnformatted("Assistant…");ImGui::TextWrapped("%s",selected->value("partial","").c_str());}
            if(!selected->value("error","").empty())ImGui::TextWrapped("%s",selected->value("error","").c_str());
            if(selected->value("error","").find("Legacy")!=std::string::npos&&ImGui::Button("Reprendre dans une nouvelle session"))command("fork");
        }else{
            ImGui::TextWrapped("Discutez avec les fournisseurs CyMCPSDK. Configurez le fournisseur, puis ouvrez une conversation.");
            if(ImGui::Button("Nouvelle conversation")){command("configure",{{"settings",settings()}});command("new");}
        }
        ImGui::EndChild();
        if(attachment.is_object()){
            ImGui::TextWrapped("Page jointe : %s",attachment.value("title","").c_str());
            if(ImGui::SmallButton("Retirer la page"))command("remove_attachment");
        }
        ImGui::BeginDisabled(!has||busy);
        if(ImGui::SmallButton("Joindre la page active"))command("attach",{{"tabId",model_->selected},{"url",model_->active().current().url}});
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextMultiline("##assistant-message",assistant_draft_.data(),assistant_draft_.size(),{-1,66*scale_});editing_|=ImGui::IsItemActive();
        if(ImGui::Button("Envoyer")&&assistant_draft_[0]){command("send",{{"text",assistant_draft_.data()}});assistant_draft_[0]=0;}
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(!busy);
        if(ImGui::Button("Arrêter"))command("cancel");ImGui::EndDisabled();
    }
    if(!state.value("error","").empty())ImGui::TextWrapped("%s",state.value("error","").c_str());
    ImGui::EndChild();
    ImGui::PopStyleVar();
}
}
