// -*- coding: utf-8 -*-
// Copyright (C) 2013 Rosen Diankov <rosen.diankov@gmail.com>
//
// This file is part of OpenRAVE.
// OpenRAVE is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include "jsoncommon.h"

#include <openrave/openravejson.h>
#include <openrave/openravemsgpack.h>
#include <fstream>
#include <rapidjson/writer.h>
#include <rapidjson/ostreamwrapper.h>

namespace OpenRAVE {

class EnvironmentJSONWriter
{
public:
    EnvironmentJSONWriter(const AttributesList& atts, rapidjson::Value& rEnvironment, rapidjson::Document::AllocatorType& allocator) : _rEnvironment(rEnvironment), _allocator(allocator) {
        FOREACHC(itatt,atts) {
            if( itatt->first == "openravescheme" ) {
                _vForceResolveOpenRAVEScheme = itatt->second;
            }
        }
    }

    virtual ~EnvironmentJSONWriter() {
    }

    virtual void Write(EnvironmentBasePtr penv) {
        std::vector<KinBodyPtr> vbodies;
        penv->GetBodies(vbodies);
        std::list<KinBodyPtr> listbodies(vbodies.begin(), vbodies.end());
        _Write(listbodies);
    }

    virtual void Write(KinBodyPtr pbody) {
        std::list<KinBodyPtr> listbodies;
        listbodies.push_back(pbody);
        _Write(listbodies);
    }

    virtual void Write(const std::list<KinBodyPtr>& listbodies) {
        _Write(listbodies);
    }

protected:

    virtual void _Write(const std::list<KinBodyPtr>& listbodies) {
        _rEnvironment.SetObject();
        _mapBodyIds.clear();
        if (listbodies.size() > 0) {
            EnvironmentBaseConstPtr penv = listbodies.front()->GetEnv();
            OpenRAVE::JSON::SetJsonValueByKey(_rEnvironment, "unit", penv->GetUnit(), _allocator);

            int globalId = 0;
            FOREACHC(itbody, listbodies) {
                BOOST_ASSERT((*itbody)->GetEnv() == penv);
                BOOST_ASSERT(_mapBodyIds.find((*itbody)->GetEnvironmentId()) == _mapBodyIds.end());
                _mapBodyIds[(*itbody)->GetEnvironmentId()] = globalId++;
            }

            rapidjson::Value bodiesValue;
            bodiesValue.SetArray();

            FOREACHC(itbody,listbodies) {
                KinBodyPtr pbody = *itbody;

                rapidjson::Value bodyValue;
                bodyValue.SetObject();

                // dof value
                std::vector<dReal> vDOFValues;
                pbody->GetDOFValues(vDOFValues);
                if (vDOFValues.size() > 0) {
                    rapidjson::Value dofValues;
                    dofValues.SetArray();
                    dofValues.Reserve(vDOFValues.size(), _allocator);
                    for(size_t iDOF=0; iDOF<vDOFValues.size(); iDOF++) {
                        rapidjson::Value jointDOFValue;
                        KinBody::JointPtr pJoint = pbody->GetJointFromDOFIndex(iDOF);
                        std::string jointId = pJoint->GetInfo()._id;
                        if (jointId.empty()) {
                            jointId = pJoint->GetInfo()._name;
                        }
                        OpenRAVE::JSON::SetJsonValueByKey(jointDOFValue, "jointId", jointId, _allocator);
                        OpenRAVE::JSON::SetJsonValueByKey(jointDOFValue, "value", vDOFValues[iDOF], _allocator);
                        dofValues.PushBack(jointDOFValue, _allocator);
                    }
                    OpenRAVE::JSON::SetJsonValueByKey(bodyValue, "dofValues", dofValues, _allocator);
                }

                OpenRAVE::JSON::SetJsonValueByKey(bodyValue, "name", pbody->GetName(), _allocator);
                OpenRAVE::JSON::SetJsonValueByKey(bodyValue, "transform", pbody->GetTransform(), _allocator);

                KinBody::KinBodyStateSaver saver(pbody);
                vector<dReal> vZeros(pbody->GetDOF(), 0);
                pbody->SetDOFValues(vZeros, KinBody::CLA_Nothing);
                pbody->SetTransform(Transform());

                // grabbed info
                std::vector<KinBody::GrabbedInfoPtr> vGrabbedInfo;
                pbody->GetGrabbedInfo(vGrabbedInfo);
                if (vGrabbedInfo.size() > 0) {
                    rapidjson::Value grabbedsValue;
                    grabbedsValue.SetArray();
                    FOREACHC(itgrabbedinfo, vGrabbedInfo) {
                        rapidjson::Value grabbedValue;
                        (*itgrabbedinfo)->SerializeJSON(grabbedValue, _allocator);
                        grabbedsValue.PushBack(grabbedValue, _allocator);
                    }
                    bodyValue.AddMember("grabbed", grabbedsValue, _allocator);
                }

                std::string id = (*itbody)->GetInfo()._id;
                if (id.empty()) {
                    id = str(boost::format("body%d_motion")%_mapBodyIds[pbody->GetEnvironmentId()]);
                }
                OpenRAVE::JSON::SetJsonValueByKey(bodyValue, "id", id, _allocator);
                OpenRAVE::JSON::SetJsonValueByKey(bodyValue, "name", pbody->GetName(), _allocator);
                OpenRAVE::JSON::SetJsonValueByKey(bodyValue, "referenceUri", (*itbody)->GetInfo()._referenceUri, _allocator);

                // links
                {
                    rapidjson::Value linksValue;
                    linksValue.SetArray();
                    // get current linkinfoptr vector
                    FOREACHC(itlink, pbody->GetLinks()) {
                        rapidjson::Value linkValue;
                        (*itlink)->GetInfo().SerializeJSON(linkValue, _allocator);

                        rapidjson::Value geometriesValue;
                        geometriesValue.SetArray();
                        FOREACHC(itgeom, (*itlink)->GetGeometries()) {
                            rapidjson::Value geometryValue;
                            (*itgeom)->GetInfo().SerializeJSON(geometryValue, _allocator);
                            geometriesValue.PushBack(geometryValue, _allocator);
                        }
                        if (linkValue.HasMember("geometries")) {
                            linkValue.RemoveMember("geometries");
                        }
                        linkValue.AddMember("geometries", geometriesValue, _allocator);
                        linksValue.PushBack(linkValue, _allocator);
                    }
                    if (linksValue.Size() > 0) {
                        bodyValue.AddMember("links", linksValue, _allocator);
                    }
                }

                // joints
                if (pbody->GetJoints().size() + pbody->GetPassiveJoints().size() > 0)
                {
                    rapidjson::Value jointsValue;
                    jointsValue.SetArray();
                    FOREACHC(itjoint, pbody->GetJoints()) {
                        rapidjson::Value jointValue;
                        (*itjoint)->GetInfo().SerializeJSON(jointValue, _allocator);
                        jointsValue.PushBack(jointValue, _allocator);
                    }
                    FOREACHC(itjoint, pbody->GetPassiveJoints()) {
                        rapidjson::Value jointValue;
                        (*itjoint)->GetInfo().SerializeJSON(jointValue, _allocator);
                        jointsValue.PushBack(jointValue, _allocator);
                    }
                    bodyValue.AddMember("joints", jointsValue, _allocator);
                }

                // robot
                if (pbody->IsRobot()) {
                    RobotBasePtr probot = RaveInterfaceCast<RobotBase>(pbody);
                    // manipulators
                    if (probot->GetManipulators().size() > 0) {
                        rapidjson::Value manipulatorsValue;
                        manipulatorsValue.SetArray();

                        FOREACHC(itmanip, probot->GetManipulators()) {
                            rapidjson::Value manipulatorValue;
                            (*itmanip)->UpdateAndGetInfo().SerializeJSON(manipulatorValue, _allocator);
                            manipulatorsValue.PushBack(manipulatorValue, _allocator);
                        }
                        if (manipulatorsValue.Size() > 0) {
                            bodyValue.AddMember("manipulators", manipulatorsValue, _allocator);
                        }
                    }
                    // attachedsensors
                    if (probot->GetAttachedSensors().size() > 0) {
                        rapidjson::Value attachedSensorsValue;
                        attachedSensorsValue.SetArray();
                        FOREACHC(itattachedsensor, probot->GetAttachedSensors()) {
                            rapidjson::Value attachedSensorValue;
                            (*itattachedsensor)->UpdateAndGetInfo().SerializeJSON(attachedSensorValue, _allocator);
                            attachedSensorsValue.PushBack(attachedSensorValue, _allocator);
                        }

                        if (attachedSensorsValue.Size() > 0) {
                            bodyValue.AddMember("attachedSensors", attachedSensorsValue, _allocator);
                        }
                    }
                    // connectedbodies
                    if (probot->GetConnectedBodies().size() > 0) {
                        rapidjson::Value connectedBodiesValue;
                        connectedBodiesValue.SetArray();
                        FOREACHC(itconnectedbody, probot->GetConnectedBodies()) {
                            rapidjson::Value connectedBodyValue;
                            (*itconnectedbody)->GetInfo().SerializeJSON(connectedBodyValue, _allocator);

                            // here we try to fix the uri in connected body
                            if (connectedBodyValue.HasMember("uri")) {
                                std::string uri = _CanonicalizeURI(connectedBodyValue["uri"].GetString());
                                connectedBodyValue.RemoveMember("uri");
                                OpenRAVE::JSON::SetJsonValueByKey(connectedBodyValue, "uri", uri, _allocator);
                            }
                            connectedBodiesValue.PushBack(connectedBodyValue, _allocator);
                        }
                        if (connectedBodiesValue.Size() > 0) {
                            bodyValue.AddMember("connectedBodies", connectedBodiesValue, _allocator);
                        }
                    }

                    // gripperinfo
                    if (probot->GetGripperInfos().size() > 0) {
                        rapidjson::Value gripperInfosValue;
                        gripperInfosValue.SetArray();
                        FOREACHC(itgripperinfo, probot->GetGripperInfos()) {
                            rapidjson::Value gripperInfoValue;
                            (*itgripperinfo)->SerializeJSON(gripperInfoValue, _allocator);
                            gripperInfosValue.PushBack(gripperInfoValue, _allocator);
                        }

                        if (gripperInfosValue.Size() > 0) {
                            bodyValue.AddMember("gripperInfos", gripperInfosValue, _allocator);
                        }
                    }
                    OpenRAVE::JSON::SetJsonValueByKey(bodyValue, "isRobot", true, _allocator);
                }

                // readable interface
                if (pbody->GetReadableInterfaces().size() > 0) {
                    rapidjson::Value readableInterfacesValue;
                    readableInterfacesValue.SetObject();
                    FOREACHC(it, pbody->GetReadableInterfaces()) {
                        JSONReadablePtr pReadable = OPENRAVE_DYNAMIC_POINTER_CAST<JSONReadable>(it->second);
                        if (!!pReadable) {
                            rapidjson::Value readableValue;
                            pReadable->SerializeJSON(readableValue, _allocator);
                            readableInterfacesValue.AddMember(rapidjson::Value(it->first.c_str(), _allocator).Move(), readableValue, _allocator);
                        }
                    }
                    if (readableInterfacesValue.MemberCount() > 0) {
                        bodyValue.AddMember("readableInterfaces", readableInterfacesValue, _allocator);
                    }
                }
                // finally push to the bodiesValue array if bodyValue is not empty
                if (bodyValue.MemberCount() > 0) {
                    bodiesValue.PushBack(bodyValue, _allocator);
                }
            }

            if (bodiesValue.Size() > 0) {
                _rEnvironment.AddMember("bodies", bodiesValue, _allocator);
            }
        }
    }

    /// \brief get the scheme of the uri, e.g. file: or openrave:
    void _ParseURI(const std::string& uri, std::string& scheme, std::string& path, std::string& fragment)
    {
        path = uri;
        size_t hashindex = path.find_last_of('#');
        if (hashindex != std::string::npos) {
            fragment = path.substr(hashindex + 1);
            path = path.substr(0, hashindex);
        }

        size_t colonindex = path.find_first_of(':');
        if (colonindex != std::string::npos) {
            scheme = path.substr(0, colonindex);
            path = path.substr(colonindex + 1);
        }
    }

    std::string _CanonicalizeURI(const std::string& uri)
    {
        std::string scheme, path, fragment;
        _ParseURI(uri, scheme, path, fragment);

        if (_vForceResolveOpenRAVEScheme.size() > 0 && scheme == "file") {
            // check if inside an openrave path, and if so, return the openrave relative directory instead using "openrave:"
            std::string filename;
            if (RaveInvertFileLookup(filename, path)) {
                path = "/" + filename;
                scheme = _vForceResolveOpenRAVEScheme;
            }
        }

        // TODO: fix other scheme.

        // fix extension, replace dae with json
        // this is done for ease of migration
        size_t len = path.size();
        if (len >= sizeof(".dae") - 1) {
            if (path[len - 4] == '.' && ::tolower(path[len - 3]) == 'd' && ::tolower(path[len - 2]) == 'a' && ::tolower(path[len - 1]) == 'e') {
                path = path.substr(0, path.size() - (sizeof(".dae") - 1)) + ".json";
            }
        }

        std::string newuri = scheme + ":" + path;
        if (fragment.size() > 0) {
            newuri += "#" + fragment;
        }
        return newuri;
    }

    std::string _vForceResolveOpenRAVEScheme; ///< if specified, writer will attempt to convert a local system URI (**file:/**) to a a relative path with respect to $OPENRAVE_DATA paths and use **customscheme** as the scheme

    std::map<int, int> _mapBodyIds; ///< map from body environment id to unique json ids

    rapidjson::Value& _rEnvironment;
    rapidjson::Document::AllocatorType& _allocator;
};

void RaveWriteJSONFile(EnvironmentBasePtr penv, const std::string& filename, const AttributesList& atts)
{
    std::ofstream ofstream(filename.c_str());
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(penv);
    OpenRAVE::JSON::DumpJson(doc, ofstream);
}

void RaveWriteJSONFile(KinBodyPtr pbody, const std::string& filename, const AttributesList& atts)
{
    std::ofstream ofstream(filename.c_str());
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(pbody);
    OpenRAVE::JSON::DumpJson(doc, ofstream);
}

void RaveWriteJSONFile(const std::list<KinBodyPtr>& listbodies, const std::string& filename, const AttributesList& atts)
{
    std::ofstream ofstream(filename.c_str());
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(listbodies);
    OpenRAVE::JSON::DumpJson(doc, ofstream);
}

void RaveWriteJSONStream(EnvironmentBasePtr penv, ostream& os, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(penv);
    OpenRAVE::JSON::DumpJson(doc, os);
}

void RaveWriteJSONStream(KinBodyPtr pbody, ostream& os, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(pbody);
    OpenRAVE::JSON::DumpJson(doc, os);
}

void RaveWriteJSONStream(const std::list<KinBodyPtr>& listbodies, ostream& os, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(listbodies);
    OpenRAVE::JSON::DumpJson(doc, os);
}

void RaveWriteJSONMemory(EnvironmentBasePtr penv, std::vector<char>& output, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(penv);
    OpenRAVE::JSON::DumpJson(doc, output);
}

void RaveWriteJSONMemory(KinBodyPtr pbody, std::vector<char>& output, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(pbody);
    OpenRAVE::JSON::DumpJson(doc, output);
}

void RaveWriteJSONMemory(const std::list<KinBodyPtr>& listbodies, std::vector<char>& output, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(listbodies);
    OpenRAVE::JSON::DumpJson(doc, output);
}

void RaveWriteJSON(EnvironmentBasePtr penv, rapidjson::Value& rEnvironment, rapidjson::Document::AllocatorType& allocator, const AttributesList& atts)
{
    EnvironmentJSONWriter jsonwriter(atts, rEnvironment, allocator);
    jsonwriter.Write(penv);
}

void RaveWriteJSON(KinBodyPtr pbody, rapidjson::Value& rEnvironment, rapidjson::Document::AllocatorType& allocator, const AttributesList& atts)
{
    EnvironmentJSONWriter jsonwriter(atts, rEnvironment, allocator);
    jsonwriter.Write(pbody);
}

void RaveWriteJSON(const std::list<KinBodyPtr>& listbodies, rapidjson::Value& rEnvironment, rapidjson::Document::AllocatorType& allocator, const AttributesList& atts)
{
    EnvironmentJSONWriter jsonwriter(atts, rEnvironment, allocator);
    jsonwriter.Write(listbodies);
}

void RaveWriteMsgPackFile(EnvironmentBasePtr penv, const std::string& filename, const AttributesList& atts)
{
    std::ofstream ofstream(filename.c_str());
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(penv);
    OpenRAVE::MsgPack::DumpMsgPack(doc, ofstream);
}

void RaveWriteMsgPackFile(KinBodyPtr pbody, const std::string& filename, const AttributesList& atts)
{
    std::ofstream ofstream(filename.c_str());
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(pbody);
    OpenRAVE::MsgPack::DumpMsgPack(doc, ofstream);
}

void RaveWriteMsgPackFile(const std::list<KinBodyPtr>& listbodies, const std::string& filename, const AttributesList& atts)
{
    std::ofstream ofstream(filename.c_str());
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(listbodies);
    OpenRAVE::MsgPack::DumpMsgPack(doc, ofstream);
}

void RaveWriteMsgPackStream(EnvironmentBasePtr penv, ostream& os, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(penv);
    OpenRAVE::MsgPack::DumpMsgPack(doc, os);
}

void RaveWriteMsgPackStream(KinBodyPtr pbody, ostream& os, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(pbody);
    OpenRAVE::MsgPack::DumpMsgPack(doc, os);
}

void RaveWriteMsgPackStream(const std::list<KinBodyPtr>& listbodies, ostream& os, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(listbodies);
    OpenRAVE::MsgPack::DumpMsgPack(doc, os);
}

void RaveWriteMsgPackMemory(EnvironmentBasePtr penv, std::vector<char>& output, const AttributesList& atts)
{
    rapidjson::Document doc;
    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(penv);
    OpenRAVE::MsgPack::DumpMsgPack(doc, output);
}

void RaveWriteMsgPackMemory(KinBodyPtr pbody, std::vector<char>& output, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(pbody);
    OpenRAVE::MsgPack::DumpMsgPack(doc, output);
}

void RaveWriteMsgPackMemory(const std::list<KinBodyPtr>& listbodies, std::vector<char>& output, const AttributesList& atts)
{
    rapidjson::Document doc;

    EnvironmentJSONWriter jsonwriter(atts, doc, doc.GetAllocator());
    jsonwriter.Write(listbodies);
    OpenRAVE::MsgPack::DumpMsgPack(doc, output);
}

}

