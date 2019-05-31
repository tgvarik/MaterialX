#ifndef PLUGIN_H
#define PLUGIN_H

#include <MaterialXFormat/File.h>

#include <maya/MPxNode.h>
#include <maya/MObject.h>

class Plugin
{
  public:
	static Plugin& instance();

	void initialize(const std::string& loadPath);

	MaterialX::FilePath getLibrarySearchPath() const
	{
		return _librarySearchPath;
	}

  private:
	Plugin()
	{
	}

	std::string _librarySearchPath;
};

#endif /* PLUGIN_H */
