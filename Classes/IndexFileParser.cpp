#include "IndexFileParser.h"
#include "json/reader.h"
#include "json/document.h"
#include "json/writer.h"
#include "json/stringbuffer.h"
#include <fstream>

USING_NS_CC;

const float IndexFileParser::sFrameRate = 30.f;
const std::string IndexFileParser::s_DefaultAnim = "Default Animation";
std::string IndexFileParser::_DataFileName = "config.json";

bool IndexFileParser::_InSerializing = false;

ResourceDataList IndexFileParser::s_AnimFileData;

ResourceDataList* IndexFileParser::parseIndexFile(const std::string& filePath)
{
	_DataFileName = filePath;

	std::string contentStr = FileUtils::getInstance()->getStringFromFile(filePath);

	rapidjson::Document doc;
	doc.Parse<0>(contentStr.c_str());

	if ( doc.HasParseError() || !doc.HasMember("data"))
		return nullptr;

	s_AnimFileData.clear();

	rapidjson::Value&	na = doc["data"];
	int nodeSize = na.Size();
	for (int i = 0; i < nodeSize; ++i)
	{
		rapidjson::Value& nodeValue = na[i];

		//Parse node context
		ResourceData	t;
		if (nodeValue.HasMember("model")){
			t.modelFile = nodeValue["model"].GetString();
		}

		if (nodeValue.HasMember("name")) {
			t.name = nodeValue["name"].GetString();
		}
		if (nodeValue.HasMember("tex")){
			t.texFile = nodeValue["tex"].GetString();
		}
		if (nodeValue.HasMember("anim")){
			t.animFile = nodeValue["anim"].GetString();
		}else{
			t.animFile = t.modelFile;
		}

// 		if (nodeValue.HasMember("sec") && nodeValue["sec"].IsArray()){
// 			rapidjson::Value& secValue = nodeValue["sec"][0u];
// 			for (auto itr = secValue.MemberonBegin(); itr != secValue.MemberonEnd(); ++itr){
// 				ResourceData::AnimFrames secFrame;
// 				secFrame.name = itr->name.GetString();
// 				if (itr->value.IsArray()){
// 					secFrame.start = itr->value[0u].GetInt();
// 					secFrame.end = itr->value[1].GetInt();
// 				}
// 				t.animList.push_back(secFrame);
// 			}
// 		}

		//////////////////////////////////////////////////////////////////////////
		if (nodeValue.HasMember("sec") && nodeValue["sec"].IsObject()){
			rapidjson::Value& secValue = nodeValue["sec"];
			for (auto itr = secValue.MemberonBegin(); itr != secValue.MemberonEnd(); ++itr){
				ResourceData::AnimFrames secFrame;
				secFrame.name = itr->name.GetString();
				if (itr->value.IsArray()){
					secFrame.start = itr->value[0u].GetInt();
					secFrame.end = itr->value[1].GetInt();
				}
				t.animList.push_back(secFrame);
			}
		}
		//////////////////////////////////////////////////////////////////////////


		s_AnimFileData.push_back(t);
	}

	return &s_AnimFileData;
}

ResourceData::AnimFrames* IndexFileParser::findAnim(const std::string& modelName, const std::string& animName)
{
	auto itr = s_AnimFileData.begin();
	for (; itr != s_AnimFileData.end(); ++itr)
	{
		if (itr->name == modelName)
			break;
	}

	if (itr != s_AnimFileData.end())
	{
		for (auto animItr = itr->animList.begin(); animItr != itr->animList.end(); ++animItr){
			if (animItr->name == animName)
				return &(*animItr);
		}
	}

	return nullptr;
}

ResourceData* IndexFileParser::findViewDate(const std::string& modelName)
{
	for (auto itr = s_AnimFileData.begin(); itr != s_AnimFileData.end(); ++itr)
	{
		if (itr->name == modelName)
			return &(*itr);
	}

	return nullptr;
}


ResourceData* IndexFileParser::loadNewModel(const std::string& filePath, const std::string& tex /*= ""*/)
{
	ResourceData newData;

	newData.name = filePath;

	FileUtils::getInstance()->addSearchPath("data/" + filePath);
	if (FileUtils::getInstance()->isFileExist(filePath + ".c3t")){
		newData.modelFile = filePath + ".c3t";
	}
	if (FileUtils::getInstance()->isFileExist(filePath + ".c3b")){
		newData.modelFile = filePath + ".c3b";
	}

	if (!tex.empty()){
		newData.texFile = tex;
	}

	if (newData.modelFile.empty())
		return nullptr;

	newData.animFile = newData.modelFile;

	s_AnimFileData.push_back(newData);

	return &(s_AnimFileData.back());
}

bool IndexFileParser::serializeToFile()
{
	if (_InSerializing)
		return false;

	_InSerializing = true;

	//////////////////////////////////////////////////////////////////////////

	//Serialize to json
	rapidjson::Document Doc;
	Doc.SetObject();

	rapidjson::Value vElem(rapidjson::kArrayType);
	for (int i = 0; i < s_AnimFileData.size(); ++i)
	{
		rapidjson::Value vElemItem(rapidjson::kObjectType);
		vElemItem.AddMember("name", s_AnimFileData.at(i).name.c_str(), Doc.GetAllocator());
		vElemItem.AddMember("model", s_AnimFileData.at(i).modelFile.c_str(), Doc.GetAllocator());
		if (!s_AnimFileData.at(i).texFile.empty()){
			vElemItem.AddMember("tex", s_AnimFileData.at(i).texFile.c_str(), Doc.GetAllocator());
		}
		if (!s_AnimFileData.at(i).animFile.empty()){
			vElemItem.AddMember("anim", s_AnimFileData.at(i).modelFile.c_str(), Doc.GetAllocator());
		}

		if (s_AnimFileData.at(i).animList.size() > 0){
			rapidjson::Value vAnimElemItem(rapidjson::kObjectType);
			for (auto& animElement : s_AnimFileData.at(i).animList){
				rapidjson::Value vAnimFrameSec(rapidjson::kArrayType);
				vAnimFrameSec.PushBack(animElement.start, Doc.GetAllocator());
				vAnimFrameSec.PushBack(animElement.end, Doc.GetAllocator());
				vAnimElemItem.AddMember(animElement.name.c_str(), vAnimFrameSec, Doc.GetAllocator());
			}
			vElemItem.AddMember("sec", vAnimElemItem, Doc.GetAllocator());
		}

		vElem.PushBack(vElemItem, Doc.GetAllocator());
	}
	Doc.AddMember("data", vElem, Doc.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	Doc.Accept(writer);
	std::string strJson(buffer.GetString(), buffer.Size());

	auto outputFile = FileUtils::getInstance()->fullPathForFilename(_DataFileName);

//	std::string filepath = (CCFileUtils::sharedFileUtils()->getWritablePath() + "test.json");
	std::ofstream outfile;
	outfile.open(outputFile.c_str());
	if (outfile.fail())
	{
		return	false;
	}
	outfile << strJson;
	outfile.close();

	//////////////////////////////////////////////////////////////////////////

	_InSerializing = false;

	return true;
}


