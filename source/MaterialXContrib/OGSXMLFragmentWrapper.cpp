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
OGSXMLFragmentWrapper::OGSXMLFragmentWrapper(GenContext* context) :
    _context(context)
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
    // For now just use float4 as it's generic
    //_typeMap[MaterialX::TypedValue<MaterialX::Color4>::TYPE] = "colorAlpha";
    _typeMap[MaterialX::TypedValue<MaterialX::Color4>::TYPE] = "float4";
    _typeMap[MaterialX::TypedValue<MaterialX::Vector2>::TYPE] = "float2";
    _typeMap[MaterialX::TypedValue<MaterialX::Vector3>::TYPE] = "float3";
    _typeMap[MaterialX::TypedValue<MaterialX::Vector4>::TYPE] = "float4";
}

OGSXMLFragmentWrapper::~OGSXMLFragmentWrapper()
{
    delete (static_cast<pugi::xml_document*>(_xmlDocument));
}

namespace
{ 
const string XML_NAME_STRING("name");
const string XML_VALUE_STRING("value");

const string MTLX_POSITION("i_position");
const string MTX_COLORSET("i_color_");
const string MTLX_UVSET("i_texcoord_");
const string MTLX_NORMAL("i_normal");
const string MTLX_TANGENT("i_tangent");
const string MTLX_BITANGENT("i_bitangent");

const string OGS_FRAGMENT("fragment");
const string OGS_PLUMBING("plumbing");
const string OGS_SHADERFRAGMENT("ShadeFragment");
const string OGS_PROPERTIES("properties");
const string OGS_VALUES("values");
// TODO: Texture and sampler strings need to be per language mappings 
const string OGS_TEXTURE2("texture2");
const string OGS_SAMPLER("sampler");
const string OGS_SEMANTIC("semantic");
const string OGS_FLAGS("flags");
const string OGS_POSITION_WORLD_SEMANTIC("Pw");
const string OGS_POSITION_OBJECT_SEMANTIC("Pm");
const string OGS_NORMAL_WORLD_SEMANTIC("Nw");
const string OGS_NORMAL_OBJECT_SEMANTIC("Nm");
const string OGS_COLORSET_SEMANTIC("colorset");
const string OGS_MAYA_BITANGENT_SEMANTIC("mayaBitangentIn");
const string OGS_MAYA_TANGENT_SEMANTIC("mayaTangentIn");
const string OGS_MAYA_UV_COORD_SEMANTIC("mayaUvCoordSemantic"); 
const string OGS_IS_REQUIREMENT_ONLY("isRequirementOnly");

void createOGSProperty(pugi::xml_node& propertiesNode, pugi::xml_node& valuesNode,
                                            const string& name,
                                            const string& type,
                                            const string& value,
                                            const string& semantic,
                                            const string& flags,
                                            StringMap& typeMap)
{
    // Special case filename
    const string MTLX_FILENAME("filename");

    if (type == MTLX_FILENAME)
    {
        // TODO: GLSL is not taking texture as an input argument, only a sampler
        // so mark it as a global. Not for DirectX11 we need both but could still
        // be a global.
        pugi::xml_node txt = propertiesNode.append_child(OGS_TEXTURE2.c_str());
        txt.append_attribute(XML_NAME_STRING.c_str()) = (name + "_texture").c_str();
        txt.append_attribute(OGS_FLAGS.c_str()) = OGS_IS_REQUIREMENT_ONLY.c_str();
        pugi::xml_node samp = propertiesNode.append_child(OGS_SAMPLER.c_str());
        samp.append_attribute(XML_NAME_STRING.c_str()) = (name).c_str();
        // TODO: Temporary make this a global for now to match current code generation
        samp.append_attribute(OGS_FLAGS.c_str()) = OGS_IS_REQUIREMENT_ONLY.c_str();
    }
    else
    {
        string ogsType = typeMap[type];
        if (!typeMap.count(type))
            return;

        pugi::xml_node prop = propertiesNode.append_child(ogsType.c_str());
        prop.append_attribute(XML_NAME_STRING.c_str()) = name.c_str();
        string flagValue = flags;
        if (!semantic.empty())
        {
            prop.append_attribute(OGS_SEMANTIC.c_str()) = semantic.c_str();
        }
        if (!flagValue.empty())
        {
            prop.append_attribute(OGS_FLAGS.c_str()) = flagValue.c_str();
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
                    const string& name,
                    const string& type,
                    const string& semantic,
                    StringMap& typeMap) 
{
    if (!typeMap.count(type))
        return;

    string ogsType = typeMap[type];

    pugi::xml_node prop = outputsNode.append_child(ogsType.c_str());
    prop.append_attribute(XML_NAME_STRING.c_str()) = name.c_str();
    if (!semantic.empty())
    {
        prop.append_attribute(OGS_SEMANTIC.c_str()) = semantic.c_str();
    }
}

void getFlags(const ShaderPort* /*port*/, string& flags)
{
    bool isRequirement = true; // TODO: Determine how to set this
    if (isRequirement)
    {
        flags = OGS_IS_REQUIREMENT_ONLY;
    }
}

// Based on the input uniform name get the OGS semantic to use
// for auto-binding.
void getStreamSemantic(const string& name, string& semantic)
{
    if (name.find(MTLX_POSITION) != string::npos)
    {
        // TODO: Determine how to tell if object / model space is required
        semantic = OGS_POSITION_WORLD_SEMANTIC;
    }
    else if (name.find(MTLX_UVSET) != string::npos)
    {
        semantic = OGS_MAYA_UV_COORD_SEMANTIC;
    }
    else if (name.find(MTLX_NORMAL) != string::npos)
    {
        // TODO: Determine how to tell if object / model space is required
        semantic = OGS_NORMAL_WORLD_SEMANTIC;
    }
    else if (name.find(MTLX_TANGENT) != string::npos)
    {
        // TODO: Determine how to tell if object / model space is required
        semantic = OGS_MAYA_TANGENT_SEMANTIC;
    }
    else if (name.find(MTLX_BITANGENT) != string::npos)
    {
        // TODO: Determine how to tell if object / model space is required
        semantic = OGS_MAYA_BITANGENT_SEMANTIC;
    }
    else if (name.find(MTX_COLORSET) != string::npos)
    {
        semantic = OGS_COLORSET_SEMANTIC;
    }
}
}

void OGSXMLFragmentWrapper::getVertexUniformSemantic(const string& name, string& semantic) const
{
/* This won't work as is not for GLSL since OGSFX has the string map
    ShaderGenerator& generator = _context->getShaderGenerator();
    const StringMap* semanticMap = generator.getSemanticMap();
    if (semanticMap)
    {
        auto val = semanticMap->find(name);
        if (val != semanticMap->end())
        {
            semantic = val->second;
        }
    }
*/
    const StringMap semanticMap  =
    {
        { "u_worldMatrix", "World" },
        { "u_worldInverseMatrix", "WorldInverse" },
        { "u_worldTransposeMatrix", "WorldTranspose" },
        { "u_worldInverseTransposeMatrix", "WorldInverseTranspose" },

        { "u_viewMatrix", "View" },
        { "u_viewInverseMatrix", "ViewInverse" },
        { "u_viewTransposeMatrix", "ViewTranspose" },
        { "u_viewInverseTransposeMatrix", "ViewInverseTranspose" },

        { "u_projectionMatrix", "Projection" },
        { "u_projectionInverseMatrix", "ProjectionInverse" },
        { "u_projectionTransposeMatrix", "ProjectionTranspose" },
        { "u_projectionInverseTransposeMatrix", "ProjectionInverseTranspose" },

        { "u_worldViewMatrix", "WorldView" },
        { "u_viewProjectionMatrix", "ViewProjection" },
        { "u_worldViewProjectionMatrix", "WorldViewProjection" },

        { "u_viewDirection", "ViewDirection" },
        { "u_viewPosition", "WorldCameraPosition" }
    };
    auto val = semanticMap.find(name);
    if (val != semanticMap.end())
    {
        semantic = val->second;
    }
}

#if 0 // TO DETERMINE IF STILL USEFUL
void OGSXMLFragmentWrapper::createWrapperFromNode(NodePtr node, std::vector<GenContext*> contexts)
{
    NodeDefPtr nodeDef = node->getNodeDef();
    if (!nodeDef)
    {
        return;
    }

    const string OGS_VERSION_STRING(node->getDocument()->getVersionString());

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
                    semantic = OGS_MAYA_UV_COORD_SEMANTIC;
                }
            }
        }
        string flags;
        createOGSProperty(xmlProperties, xmlValues,
            input->getName(), input->getType(), value, semantic, flags, _typeMap);
    }
    for (auto input : node->getParameters())
    {
        string value = input->getValue() ? input->getValue()->getValueString() : "";
        string flags;
        createOGSProperty(xmlProperties, xmlValues,
            input->getName(), input->getType(), value, "", flags, _typeMap);
    }

    // Scan outputs and create "outputs"
    pugi::xml_node xmlOutputs = xmlRoot.append_child("outputs");
    // Note: We don't want to attach a CM semantic here since code
    // generation should have added a transform already (i.e. mayaCMSemantic)
    semantic.clear();
    for (auto output : node->getActiveOutputs())
    {
        createOGSOutput(xmlOutputs, output->getName(), output->getType(), semantic, _typeMap);
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
            const string& code = shader->getSourceCode();

            // Need to get the actual code via shader generation.
            pugi::xml_node impl = impls.append_child("implementation");
            {
                impl.append_attribute("render") = "OGSRenderer";
                impl.append_attribute("language") = generator.getLanguage().c_str();
                impl.append_attribute("lang_version") = generator.getTarget().c_str();
            }
            pugi::xml_node func = impl.append_child("function_name");
            {
                // TODO: Figure out what is the proper function name to use
                func.append_attribute("val") = "main";// nodeDef->getName().c_str();
            }
            pugi::xml_node source = impl.append_child("source");
            {
                source.append_child(pugi::node_cdata).set_value(code.c_str());
            }
        }
    }
}
#endif

void OGSXMLFragmentWrapper::createWrapper(NodePtr node)
{
    _pathInputMap.clear();
    _pathOutputMap.clear();

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
    ShaderGenerator& generator = _context->getShaderGenerator();
    ShaderPtr shader = generator.generate(shaderName, port, *_context);
    if (!shader)
    {
        return;
    }
    const string& pixelShaderCode = shader->getSourceCode();
    if (pixelShaderCode.empty())
    {
        return;
    }

    const string OGS_VERSION_STRING(node->getDocument()->getVersionString());

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
            string flags;
            getFlags(uniform, flags);

            createOGSProperty(xmlProperties, xmlValues,
                name, typeString, value, semantic, flags, _typeMap);

            _pathInputMap[path] = name;
        }
    }

    // Set geometric inputs 
    if (shader->hasStage(Stage::VERTEX))
    {
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
                // TODO: This name isn't correct since it the vertex shader name
                // and not the pixel shader name. Need to figure out what to
                // do with code gen so that we get the correct name.
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
                // TODO: For now we assume all vertex inputs are "global properties"
                // and not part of the function argument list.
                string flags;
                getFlags(vertexInput, flags);
                if (!flags.empty())
                {
                    flags += ", ";
                }
                flags += "varyingInputParam";

                createOGSProperty(xmlProperties, xmlValues,
                    name, typeString, value, semantic, flags, _typeMap);
            }

            // Output vertex uniforms
            if (_outputVertexShader)
            {
                const VariableBlock& uniformInputs = vs.getUniformBlock(HW::PRIVATE_UNIFORMS);
                for (size_t i = 0; i < uniformInputs.size(); ++i)
                {
                    const ShaderPort* uniformInput = uniformInputs[i];
                    if (!uniformInput)
                    {
                        continue;
                    }
                    string name = uniformInput->getName();
                    if (name.empty())
                    {
                        continue;
                    }
                    ValuePtr valuePtr = uniformInput->getValue();
                    string value = valuePtr ? valuePtr->getValueString() : EMPTY_STRING;
                    string typeString = uniformInput->getType()->getName();
                    string semantic = uniformInput->getSemantic();
                    getVertexUniformSemantic(name, semantic);
                    // TODO: For now we assume all vertex inputs are "global properties"
                    // and not part of the function argument list.
                    string flags = OGS_IS_REQUIREMENT_ONLY;

                    createOGSProperty(xmlProperties, xmlValues,
                        name, typeString, value, semantic, flags, _typeMap);

                }               
            }
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
            // Note: We don't want to attach a CM semantic here since code
            // generation should have added a transform already (i.e. mayaCMSemantic)
            string semantic = v->getSemantic();
            createOGSOutput(xmlOutputs, name, typeString, semantic, _typeMap);

            _pathOutputMap[path] = name;
        }
    }

    pugi::xml_node impls = xmlRoot.append_child("implementation");

    // Need to get the actual code via shader generation.
    pugi::xml_node impl = impls.append_child("implementation");
    {
        impl.append_attribute("render") = "OGSRenderer";
        string language = generator.getLanguage();
        string language_version = generator.getTarget(); // This isn't really the version
        if (language == "genglsl")
        {
            language = "GLSL";
            language_version = "4.0";
        }
        impl.append_attribute("language") = language.c_str();
        impl.append_attribute("lang_version") = language_version.c_str();
    }
    pugi::xml_node func = impl.append_child("function_name");
    {
        // This will need to change for graphs, but should
        // work for single nodes.
        func.append_attribute("val") = "main"; //  nodeDef->getName().c_str();
    }
    if (_outputVertexShader)
    {
        const string& vertexShaderCode = shader->getSourceCode(Stage::VERTEX);
        if (!vertexShaderCode.empty())
        {
            pugi::xml_node vertexSource = impl.append_child("vertex_source");
            vertexSource.append_child(pugi::node_cdata).set_value(vertexShaderCode.c_str());
        }
    }
    pugi::xml_node pixelSource = impl.append_child("source");
    {
        // Works but is the incorrect code currently
        pixelSource.append_child(pugi::node_cdata).set_value(pixelShaderCode.c_str());
    }
}

void OGSXMLFragmentWrapper::getDocument(std::ostream& stream)
{
    static_cast<pugi::xml_document*>(_xmlDocument)->save(stream, "  ");
}

} // namespace MaterialX
