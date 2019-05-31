#include "MaterialXNode.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MStringArray.h>
#include <maya/MDGModifier.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>

#include <MaterialXCore/Document.h>
#include <MaterialXFormat/XmlIo.h>

const MTypeId MaterialXNode::s_typeId(0x00042402);
const MString MaterialXNode::s_typeName("MaterialXNode");
MObject MaterialXNode::s_materialXDocument;
MObject MaterialXNode::s_materialXElement;

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
	s_materialXDocument = typedAttr.create("materialXDocument", "mtlx", MFnData::kString, theString);
	CHECK_MSTATUS(typedAttr.setStorable(true));
	CHECK_MSTATUS(typedAttr.setReadable(true));
    CHECK_MSTATUS(typedAttr.setHidden(true));
	CHECK_MSTATUS(addAttribute(s_materialXDocument));

	s_materialXElement = typedAttr.create("target", "tar", MFnData::kString, theString);
	CHECK_MSTATUS(typedAttr.setStorable(true));
    CHECK_MSTATUS(typedAttr.setHidden(true));
    CHECK_MSTATUS(typedAttr.setReadable(true));

	CHECK_MSTATUS(addAttribute(s_materialXDocument));
	CHECK_MSTATUS(addAttribute(s_materialXElement));

	return MS::kSuccess;
}

MTypeId MaterialXNode::typeId() const
{
    return s_typeId;
}
