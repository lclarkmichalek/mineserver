#include "../../src/plugin_api.h"
#include "../../src/logtype.h"
#include "../../src/hook.h"

PLUGIN_API_EXPORT void test_3_init(mineserver_pointer_struct* mineserver)
{
  mineserver->logger.log(LogType::LOG_INFO, "plugin.test_3", "test_3_init");
  mineserver->plugin.setHook("test", new Hook0<bool>);
}

PLUGIN_API_EXPORT void test_3_shutdown(void)
{
}
