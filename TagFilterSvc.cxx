#include "GaudiKernel/IInterface.h"
#include "GaudiKernel/StatusCode.h"
#include "GaudiKernel/SvcFactory.h"
#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/ISvcLocator.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/PropertyMgr.h"
#include "GaudiKernel/SmartIF.h"
#include "GaudiKernel/IAppMgrUI.h"
#include "GaudiKernel/IProperty.h"


#include "GaudiKernel/IIncidentSvc.h"
#include "GaudiKernel/Incident.h"
#include "GaudiKernel/IIncidentListener.h"
#include "GaudiKernel/ISvcLocator.h"
#include "GaudiKernel/Bootstrap.h"

#include "TagFilterSvc/TagFilterSvc.h"
#include <iostream>
#include <fstream>
#include <string>
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include "TFile.h"
#include "TTree.h"
#include "TFormula.h"
using namespace rapidjson;


TagFilterSvc::TagFilterSvc( const string& name, ISvcLocator* svcloc) :
  Service (name, svcloc){
  // declare properties
  declareProperty("Criteria",  m_criteria="");
  declareProperty("tagFiles", m_tagFiles);
}

TagFilterSvc::~TagFilterSvc(){
}

StatusCode TagFilterSvc::queryInterface(const InterfaceID& riid, void** ppvInterface){
     if( IID_ITagFilterSvc.versionMatch(riid) ){
          *ppvInterface = static_cast<ITagFilterSvc*> (this);
     } else{
          return Service::queryInterface(riid, ppvInterface);
     }
     return StatusCode::SUCCESS;
}

StatusCode TagFilterSvc::initialize(){
  MsgStream log(messageService(), name());
  log << MSG::INFO << "TagFilterSvc::initialize()" << endreq;

  StatusCode sc = Service::initialize();
  sc = setProperties();
  return StatusCode::SUCCESS;
}


vector<string> TagFilterSvc::getDstFiles()
{
  int nFiles = m_tagFiles.size();
  std::cout<<"TagFilterSvc, number of tag files: "<<m_tagFiles.size()<<std::endl;
  vector<string> t_tagFiles;

  for(int i=0;i<nFiles;i++)
  {
    //Modified by chengzj chengzj@ihep.ac.cn
    if(m_tagFiles[i].find(".json",0)==-1){
      TFile* tagFile = new TFile(m_tagFiles[i].c_str());
      TTree* fileTree = (TTree*)tagFile->Get("File");
      std::string* dstFile = new std::string;
      fileTree->SetBranchAddress("fileName",&dstFile);
      fileTree->GetEntry(0);
      std::cout<<"dstFile: "<<*dstFile<<std::endl;
      m_dstFiles.push_back(*dstFile);
      delete dstFile;
      delete tagFile;
      t_tagFiles.push_back(m_tagFiles[i]);
    }
    else{
      ifstream fin(m_tagFiles[i].c_str(), ios::binary);
      if(!fin.is_open()){
          std::cout<<"Error opening Json File:"<<m_tagFiles[i].c_str()<<std::endl;
          continue;
      }
      string str, str_in;
      while(getline(fin,str)){
          str_in=str_in+str+'\n';
      }
      Document document;
      document.Parse<0>(str_in.c_str());
      if(document.HasParseError()){
          std::cout<<"Json File "<<m_tagFiles[i].c_str()<<"GetParseError "<<document.GetParseError()<<std::endl;
          continue;
      }
      for (Value::ConstMemberIterator itr = document.MemberBegin();itr != document.MemberEnd(); ++itr){
          std::cout<<"dstFile: "<<itr->name.GetString()<<std::endl;
          for (unsigned int m=0;m<document[itr->name.GetString()].Size();++m){
              m_dstFiles.push_back(itr->name.GetString());
              t_tagFiles.push_back(m_tagFiles[i]);
          }
      }
    }
  }
  m_tagFiles.assign(t_tagFiles.begin(),t_tagFiles.end());
  return m_dstFiles;
}


StatusCode TagFilterSvc::getVEntry(string tagFileName, std::vector<int>& ventry) {
  //Modified by chengzj
  static int CallNum=-1;
  static std::string tempFileName="";
  if(tagFileName.find(".json",0)==-1){
      TFormula f("crt",m_criteria.c_str());
      TFile* file = new TFile(tagFileName.c_str());
      TTree* tree = (TTree*)file->Get("Tag");
      int entry=-1;
      int runNo=-1;
      int eventId=-1;
      int totalCharged=-1;
      int totalNeutral=-1;
      int totalTrks=-1;
      tree->SetBranchAddress("entry",&entry);
      tree->SetBranchAddress("runNo",&runNo);
      tree->SetBranchAddress("eventId",&eventId);
      tree->SetBranchAddress("totalCharged",&totalCharged);
      tree->SetBranchAddress("totalNeutral",&totalNeutral);
      tree->SetBranchAddress("totalTrks",&totalTrks);
      for(int j=0;j<tree->GetEntries();j++)
      {
          tree->GetEntry(j);
          f.SetParameter(1,entry);
          f.SetParameter(2,runNo);
          f.SetParameter(3,eventId);
          f.SetParameter(4,totalCharged);
          f.SetParameter(5,totalNeutral);
          f.SetParameter(6,totalTrks);
          if(m_criteria!="")
          {
              if(f.Eval(0)==1)
              ventry.push_back(entry);
          }
          else
              ventry.push_back(entry);
      }
      delete file;
  }
  else{
      string JsonFilename=tagFileName;
      if(tempFileName!=tagFileName)
      {
        CallNum = -1;
        tempFileName = tagFileName;
      }
      CallNum++;
      ifstream fin(JsonFilename.c_str(), ios::binary);
      if(!fin.is_open()){
          std::cout<<"Error opening Json File:"<<JsonFilename.c_str()<<std::endl;
          return StatusCode::FAILURE;
      }
      string str, str_in;
      while(getline(fin,str)){
          str_in=str_in+str+'\n';
      }
      Document document;
      document.Parse<0>(str_in.c_str());
      int entry = -1;
      if(document.HasParseError()){
          std::cout<<"Json File "<<JsonFilename.c_str()<<"GetParseError "<<document.GetParseError()<<std::endl;
          return StatusCode::FAILURE;
      }
      Value::ConstMemberIterator itr = document.MemberBegin();
      for (int i=0;i<CallNum;i++){
          ++itr;
          if(itr==document.MemberEnd()){
              ventry.clear();
              return StatusCode::SUCCESS;
          }
      }
      for (unsigned int i=0;i<document[itr->name.GetString()].Size();++i){
              entry = document[itr->name.GetString()][i].GetInt();
              ventry.push_back(entry);
      }
  }
  return StatusCode::SUCCESS;
}

StatusCode TagFilterSvc::finalize(){
     MsgStream log(messageService(), name());
     log << MSG::INFO << "TagFilterSvc::finalize()" << endreq;
    return StatusCode::SUCCESS;
}
