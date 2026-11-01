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
        ImGui::TextWrapped("Démarrage des connecteurs CyTools…");
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
        ImGui::TextWrapped("Connecteurs CyTools · CyUEMCPSDK 0.1.8");
        ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##connector",assistant_connector_.c_str())){
            for(auto& c:state.value("connectors",Json::array()))if(ImGui::Selectable(c.value("name","").c_str(),assistant_connector_==c.value("id",""))){
                assistant_connector_=c.value("id","");assistant_transport_=c.value("transport","");assistant_model_[0]=assistant_endpoint_[0]=assistant_key_env_[0]=assistant_executable_[0]=0;
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
                bool app=assistant_transport_=="app-server";
                if(ImGui::RadioButton("App Server · outils navigateur",app))assistant_transport_="app-server";
                if(ImGui::RadioButton("CLI JSONL · conversation",!app))assistant_transport_="cli-jsonl";
            }ImGui::TreePop();
        }
        ImGui::BeginDisabled(state.value("probeBusy",false));
        if(ImGui::Button("Tester la connexion"))command("probe",{{"settings",settings()}});ImGui::SameLine();
        if(ImGui::Button("Enregistrer")){command("configure",{{"settings",settings()}});assistant_settings_=false;}
        ImGui::EndDisabled();
        const auto probe=state.value("probe",Json::object());
        if(state.value("probeBusy",false))ImGui::TextUnformatted("Vérification en cours…");
        ImGui::TextWrapped("%s",probe.value("status","").c_str());ImGui::TextWrapped("%s",probe.value("message","").c_str());
        if(probe.contains("models")&&ImGui::BeginCombo("Modèles détectés",assistant_model_.data())){
            for(auto& item:probe["models"]){auto id=item.value("model",item.value("id",""));if(ImGui::Selectable(id.c_str()))std::snprintf(assistant_model_.data(),assistant_model_.size(),"%s",id.c_str());}ImGui::EndCombo();
        }
        ImGui::Spacing();
        if(ImGui::Checkbox("Autoriser les actions sur le navigateur",&s.sdk_control))actions_.save=true;
        ImGui::TextWrapped("Lecture seule par défaut. Codex App Server peut utiliser les outils du navigateur. Les autres connecteurs reçoivent les pages que vous joignez. Un changement d’autorisation redémarre le compagnon.");
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
            int message_index=0;
            for(auto& message:selected->value("messages",Json::array())){
                auto text=message.value("text","");ImGui::TextUnformatted(message.value("role","")=="user"?"Vous":"Assistant");
                ImGui::TextWrapped("%s",text.c_str());
                if(message.contains("attachment"))ImGui::TextDisabled("Page jointe : %s",message["attachment"].value("title","").c_str());
                if(ImGui::BeginPopupContextItem(("message-"+std::to_string(message_index++)).c_str())){if(ImGui::MenuItem("Copier le message"))ImGui::SetClipboardText(text.c_str());ImGui::EndPopup();}
                ImGui::Spacing();ImGui::Separator();
            }
            if(busy){ImGui::TextUnformatted("Assistant…");ImGui::TextWrapped("%s",selected->value("partial","").c_str());}
            if(!selected->value("error","").empty())ImGui::TextWrapped("%s",selected->value("error","").c_str());
        }else{
            ImGui::TextWrapped("Discutez avec vos connecteurs CyTools. Configurez le fournisseur, puis ouvrez une conversation.");
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
