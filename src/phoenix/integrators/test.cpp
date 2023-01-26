#include "phoenix/core/integrator.h"
#include "phoenix/core/scene_class.h"
namespace Phoenix{

    class TestIntegrator : public Integrator{

    public:

        TestIntegrator(PropertyList properties)
        {

        }

        [[nodiscard]] string ToString()const override{
            return "test integrator";
        }

        Color3f Li(shared_ptr<Scene> scene,shared_ptr<Sampler> sampler,const Ray& ray) override
        {
            auto hit = scene->Trace(ray);
            if(!hit.basic.is_hit)
                return {0,0,0};
            return hit.basic.normal;
        }
    };

    PHOENIX_REGISTER_CLASS(TestIntegrator,"test");

}