// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#include "browser_ui.hpp"
#include <algorithm>
namespace cyurium {
using Json=nlohmann::json;
namespace {
std::string bytes(double value){char s[64];if(value<1024)snprintf(s,sizeof(s),"%.0f o",value);else if(value<1048576)snprintf(s,sizeof(s),"%.1f Ko",value/1024);else snprintf(s,sizeof(s),"%.1f Mo",value/1048576);return s;}
const Json empty=Json::array();
const Json& list(const BrowserServices& s,const char* key){auto it=s.state.find(key);return it!=s.state.end()&&it->is_array()?*it:empty;}
}
void BrowserUi::service_library(Page page,float width,float top,float height){
    auto& s=model_->profile->services;
    const float x=std::max(24.f,(width-850)*.5f),w=width-2*x;
    ImGui::SetCursorScreenPos({origin_.x+x*scale_,origin_.y+(top+30)*scale_});
    ImGui::BeginChild("services",{w*scale_,std::max(100.f,height-60)*scale_},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
    ImGui::TextUnformatted(page==Page::Downloads?"Téléchargements":"Extensions");ImGui::Spacing();
    if(ImGui::Button("Actualiser"))s.request("services-refresh");
    if(!s.notice.empty())ImGui::TextWrapped("%s",s.notice.c_str());
    if(page==Page::Extensions){
        ImGui::Spacing();ImGui::TextWrapped("uBlock Origin est inclus par défaut et se gère comme les autres extensions.");
        ImGui::Spacing();ImGui::TextWrapped("Installer une WebExtension signée par Mozilla depuis un fichier .xpi.");
        if(ImGui::Button("Choisir un fichier .xpi…"))s.request("extension-pick");
        ImGui::SetNextItemWidth(-110*scale_);ImGui::InputTextWithHint("##xpi","Chemin du fichier .xpi",extension_path_.data(),extension_path_.size());
        editing_|=ImGui::IsItemActive();ImGui::SameLine();
        if(ImGui::Button("Installer")&&extension_path_[0])s.request("extension-install",{{"path",extension_path_.data()}});
        if(ImGui::Button("Catalogue Mozilla"))model_->navigate("https://addons.mozilla.org/fr/firefox/extensions/");
    }
    ImGui::Spacing();ImGui::Separator();
    const auto& items=list(s,page==Page::Downloads?"downloads":"extensions");
    if(items.empty())ImGui::TextUnformatted(page==Page::Downloads?"Aucun téléchargement.":"Aucune extension installée.");
    for(const auto& item:items){
        const auto id=item.at("id");ImGui::PushID(id.dump().c_str());ImGui::Spacing();
        ImGui::TextWrapped("%s",item.value("name","").c_str());
        auto action=[&](const char* label,const char* value){if(ImGui::Button(label))s.request(page==Page::Downloads?"download-action":"extension-action",{{"id",id},{"action",value}});};
        if(page==Page::Downloads){
            const bool finished=item.value("succeeded",false),stopped=item.value("stopped",false);
            ImGui::TextWrapped("%s",item.value("url","").c_str());
            ImGui::Text("%s / %s",bytes(item.value("received",0.)).c_str(),bytes(item.value("total",0.)).c_str());
            if(!stopped){ImGui::ProgressBar(item.value("progress",0.f)/100,{-1,5*scale_});action("Pause","pause");ImGui::SameLine();action("Annuler","cancel");}
            else if(finished){ImGui::TextUnformatted("Terminé");action("Ouvrir","open");ImGui::SameLine();action("Afficher dans le dossier","reveal");
                auto name=item.value("name","");if(name.size()>=4&&name.substr(name.size()-4)==".xpi"){ImGui::SameLine();if(ImGui::Button("Installer l’extension"))s.request("extension-install",{{"path",item["path"]}});}}
            else {ImGui::TextWrapped("%s",item.value("error",std::string()).empty()?(item.value("partial",false)?"En pause":"Annulé"):item.value("error","").c_str());
                if(!item.value("blocked",false))action(item.value("partial",false)?"Reprendre":"Réessayer","resume");}
            if(stopped){ImGui::SameLine();action("Retirer de la liste","remove");}
        }else{
            ImGui::Text("Version %s · %s",item.value("version","").c_str(),item.value("incompatible",false)?"Incompatible":item.value("active",false)?"Active":"Désactivée");
            ImGui::TextWrapped("%s",item.value("description","").c_str());
            action(item.value("disabled",false)?"Activer":"Désactiver",item.value("disabled",false)?"enable":"disable");ImGui::SameLine();action("Mettre à jour","update");ImGui::SameLine();action("Désinstaller","remove");
            if(item.value("active",false)&&ImGui::Button("Ouvrir le panneau"))s.request("extension-popup",{{"id",id},{"tab",model_->selected}});
            if(ImGui::TreeNode("Autorisations")){auto p=item.value("permissions",Json::object());for(const char* key:{"permissions","origins"})if(p.contains(key)&&p[key].is_array())for(auto& value:p[key])ImGui::BulletText("%s",value.get<std::string>().c_str());ImGui::TreePop();}
            const auto options=item.value("options","");if(!options.empty()&&ImGui::Button("Options"))s.request("extension-options",{{"id",id}});
        }
        ImGui::Spacing();ImGui::Separator();ImGui::PopID();
    }
    ImGui::EndChild();
}
void BrowserUi::service_settings(int category,float x,float y,float width,float height){
    auto& s=model_->profile->services;
    ImGui::SetCursorScreenPos({origin_.x+x*scale_,origin_.y+y*scale_});
    ImGui::BeginChild("service-settings",{width*scale_,std::max(200.f,height)*scale_},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
    if(!s.notice.empty())ImGui::TextWrapped("%s",s.notice.c_str());
    if(category==1){
        if(ImGui::Checkbox("Rouvrir les fenêtres et onglets au démarrage",&s.restore_session))actions_.save=true;
#ifndef _WIN32
        ImGui::BeginDisabled();
#endif
        if(ImGui::Checkbox("Restaurer aussi les champs de formulaire",&s.restore_forms))actions_.save=true;
#ifndef _WIN32
        ImGui::EndDisabled();
#endif
        ImGui::TextWrapped("Les champs sont exclus par défaut. Sous Windows, les sauvegardes de session sont chiffrées pour votre compte. Les champs sensibles identifiés restent exclus même avec cette option.");
        ImGui::TextWrapped("La session est enregistrée localement. Après un arrêt inattendu, les onglets sont restaurés depuis la dernière sauvegarde valide.");
        if(s.recovered_session)ImGui::TextUnformatted("Session récupérée après un arrêt inattendu.");
        if(ImGui::Button("Voir les adresses saisies"))model_->open(Page::History);
    }else if(category==2){
        ImGui::TextWrapped("Les autorisations sont enregistrées par origine. Supprimer une règle permet au site de redemander l’autorisation.");
        if(ImGui::Button("Actualiser les permissions"))s.request("services-refresh");
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##permission-origin","https://exemple.fr",permission_origin_.data(),permission_origin_.size());editing_|=ImGui::IsItemActive();
        const char* names[]={"Localisation","Notifications","Caméra","Microphone","Presse-papiers","Lecture automatique","Popups","Stockage persistant","Cookies","MIDI","MIDI système","Accès au stockage intersite"};
        const char* types[]={"geo","desktop-notification","camera","microphone","clipboard-read","autoplay-media","popup","persistent-storage","cookie","midi","midi-sysex","storage-access"};
        ImGui::Combo("Autorisation",&permission_type_,names,IM_ARRAYSIZE(names));
        for(int value=1;value<=2;++value){if(value==2)ImGui::SameLine();if(ImGui::Button(value==1?"Autoriser ce site":"Bloquer ce site")&&permission_origin_[0])s.request("permission-set",{{"origin",permission_origin_.data()},{"permission",types[permission_type_]},{"value",value}});}
        ImGui::Separator();
        for(auto& p:list(s,"permissions")){
            ImGui::PushID((p.value("origin","")+p.value("type","")).c_str());
            ImGui::TextWrapped("%s",p.value("origin","").c_str());ImGui::Text("%s : %s",p.value("label","").c_str(),p.value("value",0)==1?"Autorisé":p.value("value",0)==2?"Bloqué":"Demander");
            if(ImGui::Button("Supprimer la règle"))s.request("permission-set",{{"origin",p["origin"]},{"permission",p["type"]},{"value",0}});
            ImGui::Separator();ImGui::PopID();
        }
        if(ImGui::Button("Réinitialiser toutes les permissions"))s.request("permissions-clear");
    }else{
        ImGui::TextUnformatted("Automatisation · MCP / SDK");
        if(ImGui::Checkbox("Activer le service local CyToolsCore",&s.sdk_enabled))actions_.save=true;
        ImGui::BeginDisabled(!s.sdk_enabled);
        if(ImGui::Checkbox("Autoriser le contrôle des onglets et des pages",&s.sdk_control))actions_.save=true;
        ImGui::EndDisabled();
        ImGui::TextWrapped("L’accès en lecture permet de consulter les onglets et le texte des pages. Le contrôle permet aussi de naviguer, fermer des onglets, cliquer et remplir des champs. Les clients autorisés de votre compte partagent ce profil de navigation.");
        if(auto it=s.state.find("sdk");it!=s.state.end()){
            ImGui::TextWrapped("%s",it->value("message","").c_str());
            if(it->contains("config")&&ImGui::Button("Copier la configuration MCP"))ImGui::SetClipboardText((*it)["config"].dump(2).c_str());
        }
        ImGui::TextWrapped("Le navigateur doit rester ouvert. Désactiver le service révoque immédiatement les connexions. Aucun accès depuis les sites web ou le réseau local.");
        ImGui::Spacing();ImGui::Separator();
        if(ImGui::Checkbox("Utiliser l’accélération matérielle si disponible",&s.hardware_acceleration)){actions_.save=true;s.notice="Redémarrage du moteur nécessaire pour appliquer ce réglage.";}
        ImGui::TextWrapped("Gecko choisit le GPU et le décodage vidéo selon le pilote et sa liste de compatibilité.");
        if(ImGui::Button("Redémarrer le moteur web"))s.request("engine-restart");ImGui::SameLine();
        if(ImGui::Button("Actualiser le diagnostic GPU"))s.request("graphics-refresh");
        if(auto it=s.state.find("graphics");it!=s.state.end()&&it->is_object()){
            ImGui::TextWrapped("GPU : %s",it->value("adapter","").c_str());ImGui::TextWrapped("Pilote : %s",it->value("driver","").c_str());ImGui::TextWrapped("Compositeur : %s",it->value("compositor","").c_str());
            if(ImGui::TreeNode("Détails du rendu Gecko")){ImGui::TextWrapped("%s",it->dump(2).c_str());ImGui::TreePop();}
        }
        if(auto it=s.state.find("security");it!=s.state.end()&&it->is_object()){
            ImGui::Text("Isolation des sites : %s · confinement Gecko : niveau %d",it->value("fission",false)?"active":"inactive",it->value("sandboxLevel",0));
            const auto sb=it->value("safeBrowsing",Json::object());ImGui::TextWrapped("Protection contre les sites trompeurs : %s",sb.value("initialized",false)&&sb.value("phishing",false)?"initialisée (listes de menaces)":"indisponible");
        }
        ImGui::Spacing();ImGui::Separator();ImGui::TextUnformatted("Mises à jour de CyUrium");
        bool update_busy=false;
        if(auto it=s.state.find("update");it!=s.state.end()&&it->is_object()){
            const auto status=it->value("status","");update_busy=status=="checking"||status=="downloading"||status=="verifying";
            const auto repository=it->value("repository","");
            if(!repository.empty())ImGui::TextWrapped("GitHub Releases : %s · %s",repository.c_str(),it->value("channel","")=="preview"?"Préversions":"Stable");
            if(auto result=it->find("last_result");result!=it->end()&&result->is_object()&&!result->value("success",true))ImGui::TextWrapped("La dernière installation a échoué : %s. L’ancienne version a été conservée.",result->value("error","").c_str());
            ImGui::TextWrapped("%s",it->value("message","").c_str());
            if(status=="downloading")ImGui::ProgressBar(it->value("progress",0.f)/100,{-1,6*scale_});
            if(status=="available"&&ImGui::Button("Télécharger la mise à jour"))s.request("update-download");
            const auto release=it->value("release_url","");if(!release.empty()&&ImGui::Button("Notes de version"))model_->navigate(release);
            if(status=="staged"&&ImGui::Button("Installer et redémarrer"))s.request("update-apply");
        }
        ImGui::BeginDisabled(update_busy);
        if(ImGui::Button("Rechercher des mises à jour"))s.request("update-check");
        ImGui::EndDisabled();
        ImGui::TextWrapped("Vérification automatique quotidienne quand le moteur web est actif. Le téléchargement et l’installation se font à votre demande.");
    }
    ImGui::EndChild();
}
void BrowserUi::permission_prompt(){
    auto& s=model_->profile->services;const auto& prompts=list(s,"prompts");
    if(prompts.empty()){s.prompt_window=0;prompt_id_=0;return;}
    const auto& p=prompts.front();auto tab=p.value("tab",std::uint64_t(0));
    if(tab&&std::none_of(model_->tabs.begin(),model_->tabs.end(),[&](auto& t){return t.id==tab;}))return;
    if(s.prompt_window&&s.prompt_window!=frame_.window)return;s.prompt_window=frame_.window;
    const auto id=p.at("id").get<std::uint64_t>();
    if(prompt_id_!=id){prompt_id_=id;remember_permission_=false;device_selection_.clear();ImGui::OpenPopup("Autorisation du navigateur");}
    ImGuiWindowClass wc;wc.ParentViewportId=actions_.viewport;wc.ViewportFlagsOverrideSet=ImGuiViewportFlags_NoAutoMerge|ImGuiViewportFlags_NoTaskBarIcon;
    if(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_ViewportsEnable)ImGui::SetNextWindowClass(&wc);
    ImGui::SetNextWindowPos({actions_.bounds.x+actions_.bounds.w*.5f,actions_.bounds.y+150*scale_},ImGuiCond_Appearing,{.5f,0});
    ImGui::SetNextWindowSize({480*scale_,0},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Autorisation du navigateur",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+440*scale_);
        ImGui::TextUnformatted(p.value("title","Autorisation").c_str());ImGui::Spacing();ImGui::TextWrapped("%s",p.value("origin","").c_str());
        if(p.contains("permissions"))for(auto& v:p["permissions"])ImGui::BulletText("%s",v.get<std::string>().c_str());
        if(p.value("remember",false))ImGui::Checkbox("Mémoriser pour ce site",&remember_permission_);
        Json devices=Json::array();
        if(p.contains("devices"))for(auto& group:p["devices"]){const auto& choices=group["choices"];int& selected=device_selection_[group.value("label","")];ImGui::TextUnformatted(group.value("label","").c_str());
            if(ImGui::BeginCombo(group.value("label","").c_str(),choices.empty()?"Aucun périphérique":choices[selected].value("name","").c_str())){for(std::size_t i=0;i<choices.size();++i)if(ImGui::Selectable(choices[i].value("name","").c_str(),int(i)==selected))selected=int(i);ImGui::EndCombo();}
            if(!choices.empty())devices.push_back(choices[selected]["deviceIndex"]);
        }
        auto answer=[&](bool allow){s.request("prompt-answer",{{"id",id},{"allow",allow},{"remember",remember_permission_},{"devices",devices}});ImGui::CloseCurrentPopup();};
        if(ImGui::Button("Refuser")||ImGui::IsKeyPressed(ImGuiKey_Escape))answer(false);ImGui::SameLine();if(ImGui::Button("Autoriser"))answer(true);
        ImGui::PopTextWrapPos();ImGui::EndPopup();
    }
}
}
