#include "MaterialXNode.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MStringArray.h>
#include <maya/MDGModifier.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>

#include <MaterialXCore/Document.h>
#include <MaterialXFormat/XmlIo.h>

const MTypeId MaterialXNode::MATERIALX_NODE_TYPEID(0x00042402);
const MString MaterialXNode::MATERIALX_NODE_TYPENAME("MaterialXNode");

MString MaterialXNode::DOCUMENT_ATTRIBUTE_LONG_NAME("document");
MString MaterialXNode::DOCUMENT_ATTRIBUTE_SHORT_NAME("doc");
MObject MaterialXNode::DOCUMENT_ATTRIBUTE;

MString MaterialXNode::ELEMENT_ATTRIBUTE_LONG_NAME("element");
MString MaterialXNode::ELEMENT_ATTRIBUTE_SHORT_NAME("ele");
MObject MaterialXNode::ELEMENT_ATTRIBUTE;

MaterialXNode::MaterialXNode()
{
}

MaterialXNode::~MaterialXNode()
{
}

void* MaterialXNode::creator()
{
    return new MaterialXNode();
}

MStatus MaterialXNode::initialize()
{
	MFnTypedAttribute typedAttr;
	MFnStringData stringData;

	MObject theString = stringData.create();
	DOCUMENT_ATTRIBUTE = typedAttr.create(DOCUMENT_ATTRIBUTE_LONG_NAME, DOCUMENT_ATTRIBUTE_SHORT_NAME, MFnData::kString, theString);
	CHECK_MSTATUS(typedAttr.setStorable(true));
	CHECK_MSTATUS(typedAttr.setReadable(true));
    CHECK_MSTATUS(typedAttr.setHidden(true));
	CHECK_MSTATUS(addAttribute(DOCUMENT_ATTRIBUTE));

	ELEMENT_ATTRIBUTE = typedAttr.create(ELEMENT_ATTRIBUTE_LONG_NAME, ELEMENT_ATTRIBUTE_SHORT_NAME, MFnData::kString, theString);
	CHECK_MSTATUS(typedAttr.setStorable(true));
    CHECK_MSTATUS(typedAttr.setHidden(true));
    CHECK_MSTATUS(typedAttr.setReadable(true));
	CHECK_MSTATUS(addAttribute(ELEMENT_ATTRIBUTE));

	return MS::kSuccess;
}

MTypeId MaterialXNode::typeId() const
{
    return MATERIALX_NODE_TYPEID;
}
