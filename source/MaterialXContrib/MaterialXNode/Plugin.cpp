#include "Plugin.h"
#include "CreateMaterialXNodeCmd.h"
#include "MaterialXNode.h"

#include <maya/MFnPlugin.h>
#include <maya/MDGMessage.h>
#include <maya/MDrawRegistry.h>
#include <maya/MGlobal.h>
#include <maya/MIOStream.h>

Plugin& Plugin::instance()
{
	static Plugin s_instance;
	return s_instance;
}

void Plugin::initialize(const std::string& loadPath)
{
	MaterialX::FilePath searchPath(loadPath);
	_librarySearchPath = searchPath / MaterialX::FilePath("../../libraries");
}

// Plugin configuration
//
MStatus initializePlugin(MObject obj)
{
	MFnPlugin plugin(obj, "Autodesk", "1.0", "Any");
	Plugin::instance().initialize(plugin.loadPath().asChar());

	CHECK_MSTATUS(plugin.registerCommand(
        CreateMaterialXNodeCmd::NAME,
		CreateMaterialXNodeCmd::creator,
		CreateMaterialXNodeCmd::newSyntax));

	const MString materialXNodeClassification("texture/2d:drawdb/shader/texture/2d/materialXNode");
	CHECK_MSTATUS(plugin.registerNode(
		MaterialXNode::MATERIALX_NODE_TYPENAME,
		MaterialXNode::MATERIALX_NODE_TYPEID,
		MaterialXNode::creator,
		MaterialXNode::initialize,
		MPxNode::kDependNode,
		&materialXNodeClassification));

	return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
	MFnPlugin plugin(obj);
	MStatus status;

	CHECK_MSTATUS(plugin.deregisterNode(MaterialXNode::MATERIALX_NODE_TYPEID));

	CHECK_MSTATUS(plugin.deregisterCommand(CreateMaterialXNodeCmd::NAME));

	return MS::kSuccess;
}
