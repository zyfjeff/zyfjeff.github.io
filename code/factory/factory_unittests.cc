#include "factory.h"

#include <string>

#include "gtest/gtest.h"

namespace Registry {
namespace {

class TestConfigFactory {
 public:
  virtual ~TestConfigFactory() = default;
  virtual std::string name() = 0;
};

class InstanceAConfig : public TestConfigFactory {
 public:
  virtual std::string name() {
    return "InstanceA";
  }
};

class InstanceBConfig : public TestConfigFactory {
 public:
  virtual std::string name() {
    return "InstanceB";
  }
};

REGISTER_FACTORY(InstanceAConfig, TestConfigFactory);
REGISTER_FACTORY(InstanceBConfig, TestConfigFactory);

TEST(RegistryFactoryTest, base) {
  auto instancea = FactoryRegistry<TestConfigFactory>::getFactory("InstanceA");
  ASSERT_TRUE(instancea != nullptr);
  auto instanceb = FactoryRegistry<TestConfigFactory>::getFactory("InstanceB");
  ASSERT_TRUE(instanceb != nullptr);
}

}
}  // namespace Registry