#include "vk_engine.h"

#include <iostream>

int main()
{
	try
	{
		VulkanEngine engine;

		engine.init();

		engine.run();

		engine.cleanup();
	}
	catch (std::exception const& e)
	{
		std::cerr << "Unhandled exception: " << e.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
