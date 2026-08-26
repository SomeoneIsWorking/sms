#include <System/ScenarioArchiveName.hpp>

void TScenarioArchiveName::load(JSUMemoryInputStream& stream)
{
	JDrama::TNameRef::load(stream);

	mArchiveName = stream.readString();
}
