//
// TM & (c) 2017 Lucasfilm Entertainment Company Ltd. and Lucasfilm Ltd.
// All rights reserved.  See LICENSE.txt for license.
//

#include <MaterialXContrib/OGSXMLFragmentWrapper.h>

#include <MaterialXGenShader/Shader.h>
#include <MaterialXGenShader/HwShaderGenerator.h>
#include <MaterialXGenShader/GenContext.h>

#include <MaterialXFormat/XmlIo.h>
#include <MaterialXFormat/PugiXML/pugixml.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace MaterialX
{
OGSXMLFragmentWrapper::OGSXMLFragmentWrapper()
{
    _xmlDocument = new pugi::xml_document();

    // Initialize mappings from MTLX to OGS
    _typeMap["boolean"] = "bool";
    _typeMap["float"] = "float";
    _typeMap["integer"] = "int";
    _typeMap["string"] = "string";
    _typeMap[MaterialX::TypedValue<MaterialX::Matrix44>::TYPE] = "float4x4";
    // There is no mapping for this, so binder needs to promote from 3x3 to 4x4 before binding.
    _typeMap[MaterialX::TypedValue<MaterialX::Matrix33>::TYPE] = "float4x4";
    _typeMap[MaterialX::TypedValue<MaterialX::Color2>::TYPE] = "float2";
    _typeMap[MaterialX::TypedValue<MaterialX::Color3>::TYPE] = "color";
    // To determine if this reqiures a struct creation versus allowing for colorAlpha.
    // Could also just use a float4
    _typeMap[MaterialX::TypedValue<MaterialX::Color4>::TYPE] = "colorAlpha";
    _typeMap[MaterialX::TypedValue<MaterialX::Vector2>::TYPE] = "float2";
    _typeMap[MaterialX::TypedValue<MaterialX::Vector3>::TYPE] = "float3";
    _typeMap[MaterialX::TypedValue<MaterialX::Vector4>::TYPE] = "float4";
}

OGSXMLFragmentWrapper::~OGSXMLFragmentWrapper()
{
    delete _xmlDocument;
}

namespace
{ 
const std::string XML_NAME_STRING("name");
const std::string XML_VALUE_STRING("value");

void createOGSProperty(pugi::xml_node& propertiesNode, pugi::xml_node& valuesNode,
                                            const std::string& name,
                                            const std::string& type,
                                            const std::string& value,
                                            const std::string& semantic,
                                            StringMap& typeMap)
{
    // Special case filename
    const std::string MTLX_FILENAME("filename");
    // TODO: These need to be per language mappings 
    const std::string OGS_TEXTURE2("texture2");
    const std::string OGS_SAMPLER("sampler");
    const std::string OGS_SEMANTIC("semantic");
    const std::string OGS_FLAGS("flags");

    if (type == MTLX_FILENAME)
    {
        pugi::xml_node txt = propertiesNode.append_child(OGS_TEXTURE2.c_str());
        txt.append_attribute(XML_NAME_STRING.c_str()) = name.c_str();
        pugi::xml_node samp = propertiesNode.append_child(OGS_SAMPLER.c_str());
        samp.append_attribute(XML_NAME_STRING.c_str()) = (name + "_textureSampler").c_str();
    }
    // Q: How to handle geometry streams?
    else
    {
        string ogsType = typeMap[type];
        if (!typeMap.count(type))
            return;

        pugi::xml_node prop = propertiesNode.append_child(ogsType.c_str());
        prop.append_attribute(XML_NAME_STRING.c_str()) = name.c_str();
        if (!semantic.empty())
        {
            prop.append_attribute(OGS_SEMANTIC.c_str()) = semantic.c_str();
            prop.append_attribute(OGS_FLAGS.c_str()) = "varyingInputParam";
        }

        if (!value.empty())
        {
            pugi::xml_node val = valuesNode.append_child(ogsType.c_str());
            val.append_attribute(XML_NAME_STRING.c_str()) = name.c_str();
            val.append_attribute(XML_VALUE_STRING.c_str()) = value.c_str();
        }
    }
}

// Creates output children on "outputs" node
void createOGSOutput(pugi::xml_node& outputsNode,
                    const std::string& name,
                    const std::string& type,
                    StringMap& typeMap) 
{
    if (!typeMap.count(type))
        return;

    string ogsType = typeMap[type];
    pugi::xml_node prop = outputsNode.append_child(ogsType.c_str());
    prop.append_attribute(XML_NAME_STRING.c_str()) = name.c_str();
}

// Based on the input uniform name get the OGS semantic to use
// for auto-binding.
void getStreamSemantic(const string& name, string& semantic)
{
    const std::string colorSet("i_color_");
    const std::string uvSet("i_texcoord_");
    if (name.find("i_position") != std::string::npos)
    {
        semantic = "Pw"; // "Pm" if object space
    }
    else if (name.find(uvSet) != std::string::npos)
    {
        semantic = "mayaUvCoordSemantic";
        //std::string setNumber = name.substr(uvSet.size(), name.size());
    }
    else if (name.find("i_normal") != std::string::npos)
    {
        semantic = "Nw";
    }
    else if (name.find("i_tangent") != std::string::npos)
    {
        semantic = "mayaTangentIn";
    }
    else if (name.find("i_bitangent") != std::string::npos)
    {
        semantic = "mayaBitangentIn";
    }
    else if (name.find(colorSet) != std::string::npos)
    {
        semantic = "colorset";
        //std::string setNumber = name.substr(colorSet.size(), name.size());
    }
}
}

void OGSXMLFragmentWrapper::createWrapperFromNode(NodePtr node, std::vector<GenContext*> contexts)
{
    NodeDefPtr nodeDef = node->getNodeDef();
    if (!nodeDef)
    {
        return;
    }
    const string OGS_FRAGMENT("fragment");
    const string OGS_PLUMBING("plumbing");
    const string OGS_SHADERFRAGMENT("ShadeFragment");
    const string OGS_VERSION_STRING(node->getDocument()->getVersionString());
    const string OGS_PROPERTIES("properties");
    const string OGS_VALUES("values");

    pugi::xml_node xmlRoot = static_cast<pugi::xml_document*>(_xmlDocument)->append_child(OGS_FRAGMENT.c_str());
    xmlRoot.append_attribute("name") = node->getName().c_str();
    xmlRoot.append_attribute("type") = OGS_PLUMBING.c_str();
    xmlRoot.append_attribute("class") = OGS_SHADERFRAGMENT.c_str();
    xmlRoot.append_attribute("version") = OGS_VERSION_STRING.c_str();

    // Scan inputs and parameters and create "properties" and 
    // "values" children from the nodeDef
    string semantic;
    pugi::xml_node xmlProperties = xmlRoot.append_child(OGS_PROPERTIES.c_str());
    pugi::xml_node xmlValues = xmlRoot.append_child(OGS_VALUES.c_str());
    for (auto input : node->getInputs())
    {
        string value = input->getValue() ? input->getValue()->getValueString() : "";

        GeomPropDefPtr geomprop = input->getDefaultGeomProp();
        if (geomprop)
        {
            string geomNodeDefName = "ND_" + geomprop->getGeomProp() + "_" + input->getType();
            NodeDefPtr geomNodeDef = node->getDocument()->getNodeDef(geomNodeDefName);
            if (geomNodeDef)
            {
                string geompropString = geomNodeDef->getAttribute("node");
                if (geompropString == "texcoord")
                {
                    semantic = "mayaUvCoordSemantic";
                }
            }
        }
        createOGSProperty(xmlProperties, xmlValues,
            input->getName(), input->getType(), value, semantic, _typeMap);
    }
    for (auto input : node->getParameters())
    {
        string value = input->getValue() ? input->getValue()->getValueString() : "";
        createOGSProperty(xmlProperties, xmlValues,
            input->getName(), input->getType(), value, "", _typeMap);
    }

    // Scan outputs and create "outputs"
    pugi::xml_node xmlOutputs = xmlRoot.append_child("outputs");
    for (auto output : node->getActiveOutputs())
    {
        createOGSOutput(xmlOutputs, output->getName(), output->getType(), _typeMap);
    }

    pugi::xml_node impls = xmlRoot.append_child("implementation");

    string shaderName(node->getName());
    // Work-around: Need to get a node which can be sampled. Should not be required.
    vector<PortElementPtr> samplers = node->getDownstreamPorts();
    if (!samplers.empty())
    {
        for (auto context : contexts)
        {
            PortElementPtr port = samplers[0];
            ShaderGenerator& generator = context->getShaderGenerator();
            ShaderPtr shader = generator.generate(shaderName, port, *context);
            const std::string& code = shader->getSourceCode();

            // Need to get the actual code via shader generation.
            pugi::xml_node impl = impls.append_child("implementation");
            {
                impl.append_attribute("render") = "OGSRenderer";
                impl.append_attribute("language") = generator.getLanguage().c_str();
                impl.append_attribute("lang_version") = generator.getTarget().c_str();
            }
            pugi::xml_node func = impl.append_child("function_name");
            {
                // TODO: Figure out what is the proper functio nname to use
                func.append_attribute("val") = nodeDef->getName().c_str();
            }
            pugi::xml_node source = impl.append_child("source");
            {
                source.append_child(pugi::node_cdata).set_value(code.c_str());
            }
        }
    }
}

void OGSXMLFragmentWrapper::createWrapperFromShader(NodePtr node, GenContext& context)
{
    NodeDefPtr nodeDef = node->getNodeDef();
    if (!nodeDef)
    {
        return;
    }

    // Work-around: Need to get a node which can be sampled. Should not be required.
    vector<PortElementPtr> samplers = node->getDownstreamPorts();
    if (samplers.empty())
    {
        return;
    }

    string shaderName(node->getName());
    PortElementPtr port = samplers[0];
    ShaderGenerator& generator = context.getShaderGenerator();
    ShaderPtr shader = generator.generate(shaderName, port, context);
    if (!shader)
    {
        return;
    }
    const std::string& code = shader->getSourceCode();
    if (code.empty())
    {
        return;
    }

    const string OGS_FRAGMENT("fragment");
    const string OGS_PLUMBING("plumbing");
    const string OGS_SHADERFRAGMENT("ShadeFragment");
    const string OGS_VERSION_STRING(node->getDocument()->getVersionString());
    const string OGS_PROPERTIES("properties");
    const string OGS_VALUES("values");

    pugi::xml_node xmlRoot = static_cast<pugi::xml_document*>(_xmlDocument)->append_child(OGS_FRAGMENT.c_str());
    xmlRoot.append_attribute("name") = node->getName().c_str();
    xmlRoot.append_attribute("type") = OGS_PLUMBING.c_str();
    xmlRoot.append_attribute("class") = OGS_SHADERFRAGMENT.c_str();
    xmlRoot.append_attribute("version") = OGS_VERSION_STRING.c_str();

    // Scan uniform inputs and create "properties" and "values" children.
    pugi::xml_node xmlProperties = xmlRoot.append_child(OGS_PROPERTIES.c_str());
    pugi::xml_node xmlValues = xmlRoot.append_child(OGS_VALUES.c_str());

    const ShaderStage& ps = shader->getStage(Stage::PIXEL);
    for (auto uniformsIt : ps.getUniformBlocks())
    {
        const VariableBlock& uniforms = *uniformsIt.second;
        // Skip light uniforms
        if (uniforms.getName() == HW::LIGHT_DATA)
        {
            continue;
        }

        for (size_t i=0; i<uniforms.size(); i++)
        {
            const ShaderPort* uniform = uniforms[i];
            if (!uniform)
            {
                continue;
            }
            string name = uniform->getName();
            if (name.empty())
            {
                continue;
            }

            string path = uniform->getPath();
            ValuePtr valuePtr = uniform->getValue();
            string value = valuePtr ? valuePtr->getValueString() : EMPTY_STRING;
            string typeString = uniform->getType()->getName();
            string semantic = uniform->getSemantic();
            string variable = uniform->getVariable();

            createOGSProperty(xmlProperties, xmlValues,
                name, typeString, value, semantic, _typeMap);
        }
    }

    // Set geometric inputs 
    const ShaderStage& vs = shader->getStage(Stage::VERTEX);
    const VariableBlock& vertexInputs = vs.getInputBlock(HW::VERTEX_INPUTS);
    if (!vertexInputs.empty())
    {
        for (size_t i = 0; i < vertexInputs.size(); ++i)
        {
            const ShaderPort* vertexInput = vertexInputs[i];
            if (!vertexInput)
            {
                continue;
            }
            string name = vertexInput->getName();
            if (name.empty())
            {
                continue;
            }
            ValuePtr valuePtr = vertexInput->getValue();
            string value = valuePtr ? valuePtr->getValueString() : EMPTY_STRING;
            string typeString = vertexInput->getType()->getName();
            string semantic = vertexInput->getSemantic();
            getStreamSemantic(name, semantic);

            createOGSProperty(xmlProperties, xmlValues,
                name, typeString, value, semantic, _typeMap);
        }
    }

    // Scan outputs and create "outputs"
    pugi::xml_node xmlOutputs = xmlRoot.append_child("outputs");
    for (auto uniformsIt : ps.getOutputBlocks())
    {
        const VariableBlock& uniforms = *uniformsIt.second;
        for (size_t i = 0; i < uniforms.size(); ++i)
        {
            const ShaderPort* v = uniforms[i];
            string name = v->getName();
            if (name.empty())
            {
                continue;
            }
            string path = v->getPath();
            string typeString = v->getType()->getName();
            createOGSOutput(xmlOutputs, name, typeString, _typeMap);
        }
    }

    pugi::xml_node impls = xmlRoot.append_child("implementation");

    // Need to get the actual code via shader generation.
    pugi::xml_node impl = impls.append_child("implementation");
    {
        impl.append_attribute("render") = "OGSRenderer";
        impl.append_attribute("language") = generator.getLanguage().c_str();
        impl.append_attribute("lang_version") = generator.getTarget().c_str();
    }
    pugi::xml_node func = impl.append_child("function_name");
    {
        // This will need to change for graphs, but should
        // work for single nodes.
        func.append_attribute("val") = nodeDef->getName().c_str();
    }
    pugi::xml_node source = impl.append_child("source");
    {
        // Works but is the incorrect code currently
        //source.append_child(pugi::node_cdata).set_value(code.c_str());
        source.append_child(pugi::node_cdata).set_value("// Code here");
    }

}

void OGSXMLFragmentWrapper::getDocument(std::ostream& stream)
{
    static_cast<pugi::xml_document*>(_xmlDocument)->save(stream, "  ");
}


} // namespace MaterialX
