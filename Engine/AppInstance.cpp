//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 *contact: immarespond at gmail dot com
 *
 */

#include "AppInstance.h"

#include <list>
#include <stdexcept>

#include <QDir>
#include <QtConcurrentMap>
#include <QThreadPool>

#include <boost/bind.hpp>

#include "Engine/Project.h"
#include "Engine/AppManager.h"
#include "Engine/Node.h"
#include "Engine/ViewerInstance.h"
#include "Engine/BlockingBackgroundRender.h"
#include "Engine/NodeSerialization.h"

using namespace Natron;

struct AppInstancePrivate {
    
    
    boost::shared_ptr<Natron::Project> _currentProject; //< ptr to the project
    
    int _appID; //< the unique ID of this instance (or window)
    
    
    AppInstancePrivate(int appID,AppInstance* app)
    : _currentProject(new Natron::Project(app))
    , _appID(appID)
    {
        
    }
    


};

AppInstance::AppInstance(int appID)
: _imp(new AppInstancePrivate(appID,this))
{
    appPTR->registerAppInstance(this);
    appPTR->setAsTopLevelInstance(appID);
    
    ///initialize the knobs of the project before loading anything else.
    _imp->_currentProject->initializeKnobsPublic();
}

AppInstance::~AppInstance(){
    
    appPTR->removeInstance(_imp->_appID);
    QThreadPool::globalInstance()->waitForDone();
}

void AppInstance::load(const QString& projectName,const QStringList& writers)
{
    

    ///if the app is a background project autorun and the project name is empty just throw an exception.
    if(appPTR->getAppType() == AppManager::APP_BACKGROUND_AUTO_RUN && projectName.isEmpty()){
        // cannot start a background process without a file
        throw std::invalid_argument("Project file name empty");
    }
    if (appPTR->getAppType() == AppManager::APP_BACKGROUND_AUTO_RUN) {
        
        QString realProjectName = projectName;
        int lastSep = realProjectName.lastIndexOf(QDir::separator());
        if (lastSep == -1) {
            throw std::invalid_argument("Filename has no path. It must be absolute.");
        }
        
        QString path = realProjectName.left(lastSep);
        if (!path.isEmpty() && path.at(path.size() - 1) != QDir::separator()) {
            path += QDir::separator();
        }
        
        QString name = realProjectName.remove(path);
        
        if(!_imp->_currentProject->loadProject(path,name)){
            throw std::invalid_argument("Project file loading failed.");
            
        }
        startWritersRendering(writers);
    }
    
}

boost::shared_ptr<Natron::Node> AppInstance::createNodeInternal(const QString& pluginID,int majorVersion,int minorVersion,
                                 bool requestedByLoad,bool openImageFileDialog,const NodeSerialization& serialization,bool dontLoadName)
{
    boost::shared_ptr<Node> node;
    LibraryBinary* pluginBinary = 0;
    try {
        pluginBinary = appPTR->getPluginBinary(pluginID,majorVersion,minorVersion);
    } catch (const std::exception& e) {
        Natron::errorDialog("Plugin error", std::string("Cannot load plugin executable") + ": " + e.what());
        return node;
    } catch (...) {
        Natron::errorDialog("Plugin error", std::string("Cannot load plugin executable"));
        return node;
    }
    
    if (pluginID != "Viewer") { // for now only the viewer can be an inspector.
        node.reset(new Node(this,pluginBinary));
    } else {
        node.reset(new InspectorNode(this,pluginBinary));
    }
    
    
    
    try{
        node->load(pluginID.toStdString(),node, serialization,dontLoadName);
    } catch (const std::exception& e) {
        std::string title = std::string("Exception while creating node");
        std::string message = title + " " + pluginID.toStdString() + ": " + e.what();
        qDebug() << message.c_str();
        errorDialog(title, message);
        return boost::shared_ptr<Natron::Node>();
    } catch (...) {
        std::string title = std::string("Exception while creating node");
        std::string message = title + " " + pluginID.toStdString();
        qDebug() << message.c_str();
        errorDialog(title, message);
        return boost::shared_ptr<Natron::Node>();
    }
    
    
    _imp->_currentProject->addNodeToProject(node);
    
    createNodeGui(node,requestedByLoad,openImageFileDialog);
    return node;

}

boost::shared_ptr<Natron::Node> AppInstance::createNode(const QString& name,int majorVersion,int minorVersion,bool openImageFileDialog) {
    return createNodeInternal(name, majorVersion, minorVersion, false,
                              openImageFileDialog, NodeSerialization(boost::shared_ptr<Natron::Node>()),false);
}

boost::shared_ptr<Natron::Node> AppInstance::loadNode(const QString& name,int majorVersion,int minorVersion,const NodeSerialization& serialization,bool dontLoadName) {
    return createNodeInternal(name, majorVersion, minorVersion, true, false, serialization,dontLoadName);

}

int AppInstance::getAppID() const { return _imp->_appID; }

void AppInstance::getActiveNodes(std::vector<boost::shared_ptr<Natron::Node> >* activeNodes) const{
    
    const std::vector<boost::shared_ptr<Natron::Node> > nodes = _imp->_currentProject->getCurrentNodes();
    for(U32 i = 0; i < nodes.size(); ++i){
        if(nodes[i]->isActivated()){
            activeNodes->push_back(nodes[i]);
        }
    }
}

boost::shared_ptr<Natron::Project> AppInstance::getProject() const { return _imp->_currentProject; }

boost::shared_ptr<TimeLine> AppInstance::getTimeLine() const  { return _imp->_currentProject->getTimeLine(); }



void AppInstance::connectViewersToViewerCache(){
    std::vector<boost::shared_ptr<Natron::Node> > currentNodes = _imp->_currentProject->getCurrentNodes();
    for (U32 i = 0; i < currentNodes.size(); ++i) {
        assert(currentNodes[i]);
        if(currentNodes[i]->pluginID() == "Viewer"){
            dynamic_cast<ViewerInstance*>(currentNodes[i]->getLiveInstance().get())->connectSlotsToViewerCache();
        }
    }
}

void AppInstance::disconnectViewersFromViewerCache(){
    std::vector<boost::shared_ptr<Natron::Node> > currentNodes = _imp->_currentProject->getCurrentNodes();
    for (U32 i = 0; i < currentNodes.size(); ++i) {
        assert(currentNodes[i]);
        if(currentNodes[i]->pluginID() == "Viewer"){
            dynamic_cast<ViewerInstance*>(currentNodes[i]->getLiveInstance().get())->disconnectSlotsToViewerCache();
        }
    }
}

void AppInstance::errorDialog(const std::string& title,const std::string& message) const {
    std::cout << "ERROR: " << title + ": " << message << std::endl;
}

void AppInstance::warningDialog(const std::string& title,const std::string& message) const {
    std::cout << "WARNING: " << title + ": " << message << std::endl;
}

void AppInstance::informationDialog(const std::string& title,const std::string& message) const {
    std::cout << "INFO: " << title + ": " << message << std::endl;
}

Natron::StandardButton AppInstance::questionDialog(const std::string& title,const std::string& message,Natron::StandardButtons /*buttons*/,
                                                   Natron::StandardButton /*defaultButton*/) const {
    std::cout << "QUESTION: " << title + ": " << message << std::endl;
    ///FIXME: maybe we could use scanf here... but we have to translat all the standard buttons to strings...i'm lazy
    ///to do this now.
    return Natron::Yes;
}



void AppInstance::checkViewersConnection(){
    std::vector<boost::shared_ptr<Natron::Node> > nodes = _imp->_currentProject->getCurrentNodes();
    for (U32 i = 0; i < nodes.size(); ++i) {
        assert(nodes[i]);
        if (nodes[i]->pluginID() == "Viewer") {
            boost::shared_ptr<ViewerInstance> n = boost::dynamic_pointer_cast<ViewerInstance>(nodes[i]->getLiveInstance());
            assert(n);
            n->updateTreeAndRender();
        }
    }
}

void AppInstance::redrawAllViewers() {
    std::vector<boost::shared_ptr<Natron::Node> > nodes = _imp->_currentProject->getCurrentNodes();
    for (U32 i = 0; i < nodes.size(); ++i) {
        assert(nodes[i]);
        if (nodes[i]->pluginID() == "Viewer") {
            boost::shared_ptr<ViewerInstance> n = boost::dynamic_pointer_cast<ViewerInstance>(nodes[i]->getLiveInstance());
            assert(n);
            n->redrawViewer();
        }
    }
}



void AppInstance::triggerAutoSave() {
    _imp->_currentProject->triggerAutoSave();
}

void AppInstance::startWritersRendering(const QStringList& writers){
    
    const std::vector<boost::shared_ptr<Node> > projectNodes = _imp->_currentProject->getCurrentNodes();
    
    std::vector<boost::shared_ptr<Natron::OutputEffectInstance> > renderers;
    
    if (!writers.isEmpty()) {
        for (int i = 0; i < writers.size(); ++i) {
            boost::shared_ptr<Node> node ;
            for (U32 j = 0; j < projectNodes.size(); ++j) {
                if(projectNodes[j]->getName() == writers.at(i).toStdString()){
                    node = projectNodes[j];
                    break;
                }
            }
            if(!node){
                std::string exc(writers.at(i).toStdString());
                exc.append(" does not belong to the project file. Please enter a valid writer name.");
                throw std::invalid_argument(exc);
            }else{
                if(!node->isOutputNode()){
                    std::string exc(writers.at(i).toStdString());
                    exc.append(" is not an output node! It cannot render anything.");
                    throw std::invalid_argument(exc);
                }
                if(node->pluginID() == "Viewer"){
                    throw std::invalid_argument("Internal issue with the project loader...viewers should have been evicted from the project.");
                }
                renderers.push_back(boost::dynamic_pointer_cast<OutputEffectInstance>(node->getLiveInstance()));
            }
        }
    }else{
        //start rendering for all writers found in the project
        for (U32 j = 0; j < projectNodes.size(); ++j) {
            if(projectNodes[j]->isOutputNode() && projectNodes[j]->pluginID() != "Viewer"){
                renderers.push_back(boost::dynamic_pointer_cast<OutputEffectInstance>(projectNodes[j]->getLiveInstance()));
            }
        }
        
    }
    
    if(appPTR->isBackground()){
        //blocking call, we don't want this function to return pre-maturely, in which case it would kill the app
        QtConcurrent::blockingMap(renderers,boost::bind(&AppInstance::startRenderingFullSequence,this,_1));
    }else{
        for (U32 i = 0; i < renderers.size(); ++i) {
            startRenderingFullSequence(renderers[i]);
        }
    }
}


void AppInstance::startRenderingFullSequence(boost::shared_ptr<Natron::OutputEffectInstance> writer){
    
    BlockingBackgroundRender backgroundRender(writer);
    backgroundRender.blockingRender(); //< doesn't return before rendering is finished
}



void AppInstance::clearOpenFXPluginsCaches(){
    const std::vector<boost::shared_ptr<Node> > activeNodes = _imp->_currentProject->getCurrentNodes();
    for (U32 i = 0; i < activeNodes.size(); ++i) {
        activeNodes[i]->purgeAllInstancesCaches();
    }
}

void AppInstance::quit() {
    appPTR->quit(this);
}