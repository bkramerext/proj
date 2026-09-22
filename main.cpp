#include "xmldriver.h"

int main(int argc, char* argv[])
{
    try {
        // -h can't go through normal query parsing: a lone '-' and 'h' tokenize separately
        // (only '--' is recognized), so it's handled as a raw argv check instead.
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "-h") {
                std::cout << "Example: cat tutorial/orders.csv | proj category sum[sales]" << std::endl;
                std::cout << "For more information, open README.MD." << std::endl << std::endl;
                std::cout << StreamingXml::XmlOperatorFactory::GetHelpText();
                return 0;
            }
        }

        StreamingXml::XmlDriver driver;
        bool showUsage = driver.Initialize(argc, argv);
        if (showUsage) {
            std::cout << "Example: cat tutorial/orders.csv | proj category sum[sales]" << std::endl;
            std::cout << "For more information, open README.MD." << std::endl << std::endl;
            std::cout << StreamingXml::XmlOperatorFactory::GetHelpText();
            return 0;
        }
        return driver.Run();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return -1;
    }
}
