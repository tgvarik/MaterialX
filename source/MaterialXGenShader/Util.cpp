//
// TM & (c) 2017 Lucasfilm Entertainment Company Ltd. and Lucasfilm Ltd.
// All rights reserved.  See LICENSE.txt for license.
//

#include <MaterialXGenShader/Util.h>

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

string removeExtension(const string& filename)
{
    size_t lastDot = filename.find_last_of('.');
    if (lastDot == string::npos) return filename;
    return filename.substr(0, lastDot);
}

bool readFile(const string& filename, string& contents)
{
    std::ifstream file(filename, std::ios::in);
    if (file)
    {
        StringStream stream;
        stream << file.rdbuf();
        file.close();
        if (stream)
        {
            contents = stream.str();
            return (contents.size() > 0);
        }
        return false;
    }
    return false;
}

void loadDocuments(const FilePath& rootPath, const StringSet& skipFiles, const StringSet& includeFiles,
                   vector<DocumentPtr>& documents, StringVec& documentsPaths, StringVec& errors)
{
    errors.clear();
    for (const FilePath& dir : rootPath.getSubDirectories())
    {
        for (const FilePath& file : dir.getFilesInDirectory(MTLX_EXTENSION))
        {
            if (!skipFiles.count(file) && 
               (includeFiles.empty() || includeFiles.count(file)))
            {
                DocumentPtr doc = createDocument();
                const FilePath filePath = dir / file;
                try
                {
                    readFromXmlFile(doc, filePath, dir);
                    documents.push_back(doc);
                    documentsPaths.push_back(filePath.asString());
                }
                catch (Exception& e)
                {
                    errors.push_back("Failed to load: " + filePath.asString() + ". Error: " + e.what());
                }
            }
        }
    }
}

namespace
{
    const float EPS_ZERO = 0.00001f;
    const float EPS_ONE  = 1.0f - EPS_ZERO;

    inline bool isZero(float v)
    {
        return v < EPS_ZERO;
    }

    inline bool isOne(float v)
    {
        return v > EPS_ONE;
    }

    inline bool isBlack(const Color3& c)
    {
        return isZero(c[0]) && isZero(c[1]) && isZero(c[2]);
    }

    inline bool isWhite(const Color3& c)
    {
        return isOne(c[0]) && isOne(c[1]) && isOne(c[2]);
    }

    bool isTransparentShaderGraph(OutputPtr output, const ShaderGenerator& shadergen)
    {
        // Track how many nodes has the potential of being transparent
        // and how many of these we can say for sure are 100% opaque.
        size_t numCandidates = 0;
        size_t numOpaque = 0;

        for (GraphIterator it = output->traverseGraph().begin(); it != GraphIterator::end(); ++it)
        {
            ElementPtr upstreamElem = it.getUpstreamElement();
            if (!upstreamElem)
            {            
                it.setPruneSubgraph(true);
                continue;
            }

            const string& typeName = upstreamElem->asA<TypedElement>()->getType();
            const TypeDesc* type = TypeDesc::get(typeName);
            bool isFourChannelOutput = type == Type::COLOR4 || type == Type::VECTOR4;
            if (type != Type::SURFACESHADER && type != Type::BSDF && !isFourChannelOutput)
            {
                it.setPruneSubgraph(true);
                continue;
            }

            if (upstreamElem->isA<Node>())
            {
                NodePtr node = upstreamElem->asA<Node>();

                const string& nodetype = node->getCategory();
                if (nodetype == "surface")
                {
                    // This is a candidate for transparency
                    ++numCandidates;

                    bool opaque = false;

                    InputPtr opacity = node->getInput("opacity");
                    if (!opacity)
                    {
                        opaque = true;
                    }
                    else if (opacity->getNodeName() == EMPTY_STRING && opacity->getInterfaceName() == EMPTY_STRING)
                    {
                        ValuePtr value = opacity->getValue();
                        if (!value || (value->isA<float>() && isOne(value->asA<float>())))
                        {
                            opaque = true;
                        }
                    }

                    if (opaque)
                    {
                        ++numOpaque;
                    }
                }
                else if (nodetype == "dielectricbtdf")
                {
                    // This is a candidate for transparency
                    ++numCandidates;

                    bool opaque = false;

                    // First check the weight
                    InputPtr weight = node->getInput("weight");
                    if (weight && weight->getNodeName() == EMPTY_STRING && weight->getInterfaceName() == EMPTY_STRING)
                    {
                        // Unconnected, check the value
                        ValuePtr value = weight->getValue();
                        if (value && value->isA<float>() && isZero(value->asA<float>()))
                        {
                            opaque = true;
                        }
                    }

                    if (!opaque)
                    {
                        // Second check the tint
                        InputPtr tint = node->getInput("tint");
                        if (tint && tint->getNodeName() == EMPTY_STRING && tint->getInterfaceName() == EMPTY_STRING)
                        {
                            // Unconnected, check the value
                            ValuePtr value = tint->getValue();
                            if (!value || (value->isA<Color3>() && isBlack(value->asA<Color3>())))
                            {
                                opaque = true;
                            }
                        }
                    }

                    if (opaque)
                    {
                        ++numOpaque;
                    }
                }
                else if (nodetype == "standard_surface")
                {
                    // This is a candidate for transparency
                    ++numCandidates;

                    bool opaque = false;

                    // First check the transmission weight
                    InputPtr transmission = node->getInput("transmission");
                    if (!transmission)
                    {
                        opaque = true;
                    }
                    else if (transmission->getNodeName() == EMPTY_STRING && transmission->getInterfaceName() == EMPTY_STRING)
                    {
                        // Unconnected, check the value
                        ValuePtr value = transmission->getValue();
                        if (!value || (value->asA<float>() && isZero(value->asA<float>())))
                        {
                            opaque = true;
                        }
                    }

                    // Second check the opacity
                    if (opaque)
                    {
                        opaque = false;

                        InputPtr opacity = node->getInput("opacity");
                        if (!opacity)
                        {
                            opaque = true;
                        }
                        else if (opacity->getNodeName() == EMPTY_STRING && opacity->getInterfaceName() == EMPTY_STRING)
                        {
                            // Unconnected, check the value
                            ValuePtr value = opacity->getValue();
                            if (!value || (value->isA<Color3>() && isWhite(value->asA<Color3>())))
                            {
                                opaque = true;
                            }
                        }
                    }

                    if (opaque)
                    {
                        // We know for sure this is opaque
                        ++numOpaque;
                    }
                }
                else
                {
                    // If node is nodedef which references a node graph.
                    // If so, then try to examine that node graph.
                    NodeDefPtr nodeDef = node->getNodeDef();
                    if (nodeDef)
                    {
                        const TypeDesc* nodeDefType = TypeDesc::get(nodeDef->getType());
                        if (nodeDefType == Type::BSDF)
                        {
                            InterfaceElementPtr impl = nodeDef->getImplementation(shadergen.getTarget(), shadergen.getLanguage());
                            if (impl && impl->isA<NodeGraph>())
                            {
                                NodeGraphPtr graph = impl->asA<NodeGraph>();

                                vector<OutputPtr> outputs = graph->getActiveOutputs();
                                if (outputs.size() > 0)
                                {
                                    const OutputPtr& graphOutput = outputs[0];
                                    bool isTransparent = isTransparentShaderGraph(graphOutput, shadergen);
                                    if (isTransparent)
                                    {
                                        return true;
                                    }
                                }
                            }
                        }
                        else if (isFourChannelOutput)
                        {
                            ++numCandidates;
                        }
                    }
                }

                if (numOpaque != numCandidates)
                {
                    // We found at least one candidate that we can't 
                    // say for sure is opaque. So we might need transparency.
                    return true;
                }
            }
        }

        return numCandidates > 0 ? numOpaque != numCandidates : false;
    }
}

bool isTransparentSurface(ElementPtr element, const ShaderGenerator& shadergen)
{
    if (element->isA<ShaderRef>())
    {
        ShaderRefPtr shaderRef = element->asA<ShaderRef>();
        NodeDefPtr nodeDef = shaderRef->getNodeDef();
        if (!nodeDef)
        {
            throw ExceptionShaderGenError("Could not find a nodedef for shaderref '" + shaderRef->getName() + "' in material " + shaderRef->getParent()->getName());
        }
        if (TypeDesc::get(nodeDef->getType()) != Type::SURFACESHADER)
        {
            return false;
        }

        const string& nodetype = nodeDef->getNodeString();
        if (nodetype == "standard_surface")
        {
            bool opaque = false;

            // First check the transmission weight
            BindInputPtr transmission = shaderRef->getBindInput("transmission");
            if (!transmission)
            {
                opaque = true;
            }
            else if (transmission->getOutputString() == EMPTY_STRING)
            {
                // Unconnected, check the value
                ValuePtr value = transmission->getValue();
                if (!value || isZero(value->asA<float>()))
                {
                    opaque = true;
                }
            }

            // Second check the opacity
            if (opaque)
            {
                opaque = false;

                BindInputPtr opacity = shaderRef->getBindInput("opacity");
                if (!opacity)
                {
                    opaque = true;
                }
                else if (opacity->getOutputString() == EMPTY_STRING)
                {
                    // Unconnected, check the value
                    ValuePtr value = opacity->getValue();
                    if (!value || (value->isA<Color3>() && isWhite(value->asA<Color3>())))
                    {
                        opaque = true;
                    }
                }
            }

            return !opaque;
        }
        else
        {
            InterfaceElementPtr impl = nodeDef->getImplementation(shadergen.getTarget(), shadergen.getLanguage());
            if (!impl)
            {
                throw ExceptionShaderGenError("Could not find a matching implementation for node '" + nodeDef->getNodeString() +
                    "' matching language '" + shadergen.getLanguage() + "' and target '" + shadergen.getTarget() + "'");
            }

            if (impl->isA<NodeGraph>())
            {
                NodeGraphPtr graph = impl->asA<NodeGraph>();

                vector<OutputPtr> outputs = graph->getActiveOutputs();
                if (outputs.size() > 0)
                {
                    const OutputPtr& output = outputs[0];
                    if (TypeDesc::get(output->getType()) == Type::SURFACESHADER)
                    {
                        return isTransparentShaderGraph(output, shadergen);
                    }
                }
            }
        }
    }
    else if (element->isA<Output>())
    {
        OutputPtr output = element->asA<Output>();
        return isTransparentShaderGraph(output, shadergen);
    }

    return false;
}

void mapValueToColor(const ValuePtr value, Color4& color)
{
    color = { 0.0, 0.0, 0.0, 1.0 };
    if (!value)
    {
        return;
    }
    if (value->isA<float>())
    {
        color[0] = value->asA<float>();
    }
    else if (value->isA<Color2>())
    {
        Color2 v = value->asA<Color2>();
        color[0] = v[0];
        color[3] = v[1]; // Component 2 maps to alpha
    }
    else if (value->isA<Color3>())
    {
        Color3 v = value->asA<Color3>();
        color[0] = v[0];
        color[1] = v[1];
        color[2] = v[2];
    }
    else if (value->isA<Color4>())
    {
        color = value->asA<Color4>();
    }
    else if (value->isA<Vector2>())
    {
        Vector2 v = value->asA<Vector2>();
        color[0] = v[0];
        color[1] = v[1];
    }
    else if (value->isA<Vector3>())
    {
        Vector3 v = value->asA<Vector3>();
        color[0] = v[0];
        color[1] = v[1];
        color[2] = v[2];
    }
    else if (value->isA<Vector4>())
    {
        Vector4 v = value->asA<Vector4>();
        color[0] = v[0];
        color[1] = v[1];
        color[2] = v[2];
        color[3] = v[3];
    }
}

bool requiresImplementation(const NodeDefPtr nodeDef)
{
    if (!nodeDef)
    {
        return false;
    }
    static string TYPE_NONE("none");
    const string& typeAttribute = nodeDef->getType();
    return !typeAttribute.empty() && typeAttribute != TYPE_NONE;
}

bool elementRequiresShading(const TypedElementPtr element)
{
    string elementType(element->getType());
    static StringSet colorClosures =
    {
        "surfaceshader", "volumeshader", "lightshader",
        "BSDF", "EDF", "VDF"
    };
    return (element->isA<ShaderRef>() ||
            colorClosures.count(elementType) > 0);
}

void findRenderableElements(const DocumentPtr& doc, std::vector<TypedElementPtr>& elements, bool includeReferencedGraphs)
{
    std::unordered_set<OutputPtr> processedOutputs;

    for (auto material : doc->getMaterials())
    {
        for (auto shaderRef : material->getShaderRefs())
        {
            if (!shaderRef->hasSourceUri())
            {
                // Add in all shader references which are not part of a node definition library
                NodeDefPtr nodeDef = shaderRef->getNodeDef();
                if (!nodeDef)
                {
                    throw ExceptionShaderGenError("Could not find a nodedef for shaderref '" + shaderRef->getName() + "' in material '" + shaderRef->getParent()->getName() + "'");
                }
                if (requiresImplementation(nodeDef))
                {
                    elements.push_back(shaderRef);
                }

                if (!includeReferencedGraphs)
                {
                    // Track outputs already used by the shaderref
                    for (auto bindInput : shaderRef->getBindInputs())
                    {
                        OutputPtr outputPtr = bindInput->getConnectedOutput();
                        if (outputPtr)
                        {
                            processedOutputs.insert(outputPtr);
                        }
                    }
                }
            }
        }
    }

    // Find node graph outputs. Skip any light shaders
    const string LIGHT_SHADER("lightshader");
    for (NodeGraphPtr nodeGraph : doc->getNodeGraphs())
    {
        // Skip anything from an include file including libraries.
        if (!nodeGraph->hasSourceUri())
        {
            for (OutputPtr output : nodeGraph->getOutputs())
            {
                if (output->hasSourceUri() || processedOutputs.count(output))
                {
                    continue;
                }
                NodePtr node = output->getConnectedNode();
                if (node && node->getType() != LIGHT_SHADER)
                {
                    NodeDefPtr nodeDef = node->getNodeDef();
                    if (!nodeDef)
                    {
                        throw ExceptionShaderGenError("Could not find a nodedef for node '" + node->getNamePath() + "'");
                    }
                    if (requiresImplementation(nodeDef))
                    {
                        elements.push_back(output);
                    }
                }
                processedOutputs.insert(output);
            }
        }
    }

    // Add in all top-level outputs not already processed.
    for (OutputPtr output : doc->getOutputs())
    {
        if (!output->hasSourceUri() && !processedOutputs.count(output))
        {
            elements.push_back(output);
        }
    }
}

ValueElementPtr findNodeDefChild(const string& path, DocumentPtr doc, const string& target)
{
    if (path.empty() || !doc)
    {
        return nullptr;
    }
    ElementPtr pathElement = doc->getDescendant(path);
    if (!pathElement || pathElement == doc)
    {
        return nullptr;
    }
    ElementPtr parent = pathElement->getParent();
    if (!parent || parent == doc)
    {
        return nullptr;
    }

    // Note that we must cast to a specific type derived instance as getNodeDef() is not
    // a virtual method which is overridden in derived classes.
    NodeDefPtr nodeDef = nullptr;
    ShaderRefPtr shaderRef = parent->asA<ShaderRef>();
    if (shaderRef)
    {
        nodeDef = shaderRef->getNodeDef();
    }
    else
    {
        NodePtr node = parent->asA<Node>();
        if (node)
        {
            nodeDef = node->getNodeDef(target);
        }
    }
    if (!nodeDef)
    {
        return nullptr;
    }

    // Use the path element name to look up in the equivalent element
    // in the nodedef as only the nodedef elements contain the information.
    const string& valueElementName = pathElement->getName();
    ValueElementPtr valueElement = nodeDef->getActiveValueElement(valueElementName);

    return valueElement;
}

namespace
{
    void createOGSProperty(pugi::xml_node& propertiesNode, pugi::xml_node& valuesNode,
        const std::string& name,
        const std::string& type,
        const std::string& value,
        const std::string& semantic,
        StringMap& typeMap)
    {
        // Special case filename
        if (type == "filename")
        {
            pugi::xml_node txt = propertiesNode.append_child("texture2");
            txt.append_attribute("name") = name.c_str();
            pugi::xml_node samp = propertiesNode.append_child("sampler");
            samp.append_attribute("name") = (name + "_textureSampler").c_str();
        }
        // Q: How to handle geometry streams?
        else
        {
            string ogsType = typeMap[type];
            if (!typeMap.count(type))
                return;

            pugi::xml_node prop = propertiesNode.append_child(ogsType.c_str());
            prop.append_attribute("name") = name.c_str();
            if (!semantic.empty())
            {
                prop.append_attribute("semantic") = semantic.c_str();
                prop.append_attribute("flags") = "varyingInputParam";
            }

            if (!value.empty())
            {
                pugi::xml_node val = valuesNode.append_child(ogsType.c_str());
                val.append_attribute("name") = name.c_str();
                val.append_attribute("value") = value.c_str();
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
        prop.append_attribute("name") = name.c_str();
    }
}

void createOGSWrapper(NodePtr node, std::vector<GenContext*> contexts, std::ostream& stream)
{
    NodeDefPtr nodeDef = node->getNodeDef();
    if (!nodeDef)
    {
        return;
    }

    // Make from MTLX to OGS types
    static StringMap typeMap;
    typeMap["boolean"] = "bool";
    typeMap["float"] = "float";
    typeMap["integer"] = "int";
    typeMap["string"] = "string";
    typeMap[MaterialX::TypedValue<MaterialX::Matrix44>::TYPE] = "float4x4";
    // There is no mapping for this, so binder needs to promote from 3x3 to 4x4 before binding.
    typeMap[MaterialX::TypedValue<MaterialX::Matrix33>::TYPE] = "float4x4";
    typeMap[MaterialX::TypedValue<MaterialX::Color2>::TYPE] = "float2";
    typeMap[MaterialX::TypedValue<MaterialX::Color3>::TYPE] = "color";
    // To determine if this reqiures a struct creation versus allowing for colorAlpha.
    // Could also just use a float4
    typeMap[MaterialX::TypedValue<MaterialX::Color4>::TYPE] = "colorAlpha";
    typeMap[MaterialX::TypedValue<MaterialX::Vector2>::TYPE] = "float2";
    typeMap[MaterialX::TypedValue<MaterialX::Vector3>::TYPE] = "float3";
    typeMap[MaterialX::TypedValue<MaterialX::Vector4>::TYPE] = "float4";

    pugi::xml_document xmlDoc;
    const string OGS_FRAGMENT("fragment");
    const string OGS_PLUMBING("plumbing");
    const string OGS_SHADERFRAGMENT("ShadeFragment");
    const string OGS_VERSION_STRING(node->getDocument()->getVersionString());
    const string OGS_PROPERTIES("properties");
    const string OGS_VALUES("values");

    pugi::xml_node xmlRoot = xmlDoc.append_child(OGS_FRAGMENT.c_str());
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
            input->getName(), input->getType(), value, semantic, typeMap);
    }
    for (auto input : node->getParameters())
    {
        string value = input->getValue() ? input->getValue()->getValueString() : "";
        createOGSProperty(xmlProperties, xmlValues,
            input->getName(), input->getType(), value, "", typeMap);
    }

    // Scan outputs and create "outputs"
    pugi::xml_node xmlOutputs = xmlRoot.append_child("outputs");
    for (auto output : node->getActiveOutputs())
    {
        createOGSOutput(xmlOutputs, output->getName(), output->getType(), typeMap);
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

    xmlDoc.save(stream, "  ");
}

void createOGSWrapperFromShader(NodePtr node, GenContext& context, std::ostream& stream)
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

    // Make from MTLX to OGS types
    static StringMap typeMap;
    typeMap["boolean"] = "bool";
    typeMap["float"] = "float";
    typeMap["integer"] = "int";
    typeMap["string"] = "string";
    typeMap[MaterialX::TypedValue<MaterialX::Matrix44>::TYPE] = "float4x4";
    // There is no mapping for this, so binder needs to promote from 3x3 to 4x4 before binding.
    typeMap[MaterialX::TypedValue<MaterialX::Matrix33>::TYPE] = "float4x4";
    typeMap[MaterialX::TypedValue<MaterialX::Color2>::TYPE] = "float2";
    typeMap[MaterialX::TypedValue<MaterialX::Color3>::TYPE] = "color";
    // To determine if this reqiures a struct creation versus allowing for colorAlpha.
    // Could also just use a float4
    typeMap[MaterialX::TypedValue<MaterialX::Color4>::TYPE] = "colorAlpha";
    typeMap[MaterialX::TypedValue<MaterialX::Vector2>::TYPE] = "float2";
    typeMap[MaterialX::TypedValue<MaterialX::Vector3>::TYPE] = "float3";
    typeMap[MaterialX::TypedValue<MaterialX::Vector4>::TYPE] = "float4";

    pugi::xml_document xmlDoc;
    const string OGS_FRAGMENT("fragment");
    const string OGS_PLUMBING("plumbing");
    const string OGS_SHADERFRAGMENT("ShadeFragment");
    const string OGS_VERSION_STRING(node->getDocument()->getVersionString());
    const string OGS_PROPERTIES("properties");
    const string OGS_VALUES("values");

    pugi::xml_node xmlRoot = xmlDoc.append_child(OGS_FRAGMENT.c_str());
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
                name, typeString, value, semantic, typeMap);
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

            createOGSProperty(xmlProperties, xmlValues,
                name, typeString, value, semantic, typeMap);
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
            createOGSOutput(xmlOutputs, name, typeString, typeMap);
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

    xmlDoc.save(stream, "  ");
}

} // namespace MaterialX
