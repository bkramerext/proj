#include "xmldriver.h"

int main(int argc, char* argv[])
{
    try {
        // -h is handled directly, bypassing normal query parsing entirely: it's a raw single-dash
        // flag, not an identifier the query grammar could ever tokenize as one piece (a lone '-'
        // and 'h' are separate tokens; only '--' becomes a recognized "Option" token). help[] and
        // its "usage" synonym remain the way to request this from inside a query, unaffected.
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
