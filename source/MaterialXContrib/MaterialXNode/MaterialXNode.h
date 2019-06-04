#ifndef MATERIALXNODE_H
#define MATERIALXNODE_H

#include <maya/MPxNode.h>
#include <maya/MObject.h>

class MaterialXNode : public MPxNode
{
  public:
	MaterialXNode();
	~MaterialXNode() override;

	static void* creator();
	static MStatus initialize();
	MTypeId	typeId() const override;
	SchedulingType schedulingType() const override;

	static const MTypeId MATERIALX_NODE_TYPEID;
	static const MString MATERIALX_NODE_TYPENAME;

    /// Attribute holding a MaterialX document
    static MString DOCUMENT_ATTRIBUTE_LONG_NAME;
    static MString DOCUMENT_ATTRIBUTE_SHORT_NAME;
	static MObject DOCUMENT_ATTRIBUTE;
    /// Attribute holding a MaterialX element name
    static MString ELEMENT_ATTRIBUTE_LONG_NAME;
    static MString ELEMENT_ATTRIBUTE_SHORT_NAME;
    static MObject ELEMENT_ATTRIBUTE;
	/// Attribute holding the output color of the node
	static MString OUT_COLOR_ATTRIBUTE_LONG_NAME;
	static MString OUT_COLOR_ATTRIBUTE_SHORT_NAME;
	static MObject OUT_COLOR_ATTRIBUTE;
};

#endif /* MATERIALX_NODE_H */
