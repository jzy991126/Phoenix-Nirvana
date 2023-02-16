#include "phoenix/core/integrator.h"
#include "phoenix/core/scene_class.h"
#include "phoenix/core/utils.h"

namespace Phoenix {

    class Mis : public Integrator {
    private:
        float russian_ = 0.95;
        size_t mi_recur_ = 10;
        size_t m_maxDepth = 50;
    public:

        Mis(PropertyList properties) {

        }

        [[nodiscard]] string ToString() const override {
            return "base integrator";
        }

        [[nodiscard]] Color3f Li(shared_ptr<Scene> scene, shared_ptr<Sampler> sampler, const Ray &_ray) const override {

            Interaction its;
            its = scene->Trace(_ray);
            Ray ray(_ray);
            Color3f Li(0.0f);
            bool scattered = false;
            Color3f throughput(1.0f);
            float eta = 1.0f;

            float temp = 0.f;
            size_t depth = 0;
            size_t m = 1;

            float wi1 = 0.f, wi2 = 0.f;

            while (true) {
                if (!its.basic.is_hit) {
                    break;
                }
                //temp = 0.f;
                depth += 1;

                /* Possibly include emitted radiance if requested */
                if (its.hit_type == HitType::Emitter) {

                    EmitterQueryRecord erec(ray.orig, its.basic.point, its.basic.normal);
                    Li += throughput * its.emitter->Eval(erec);
                    break;
                }

                auto bsdf = its.shape->bsdf();

                /* ==================================================================== */
                /*                     Direct illumination sampling                     */
                /* ==================================================================== */

                /* Estimate the direct illumination if this is requested */

                DirectSamplingRecord dRec(its.basic.point);

                if (!bsdf->IsSpecular()) {
                    auto value = scene->SampleEmitterDirect(dRec, sampler->Next2D());
                    if (!value.isZero()) {
                        auto emitter = dRec.emitter;
                        BSDFQueryRecord bRec(its.frame.ToLocal(-ray.dir).normalized(),
                                             its.frame.ToLocal(dRec.dir).normalized(), its.uv);
                        auto bsdf_val = bsdf->Eval(bRec);
                        if (!bsdf_val.isZero()) {
                            float bsdf_pdf = bsdf->Pdf(bRec);
                            float weight = MiWeight(dRec.pdf, bsdf_pdf);
                            temp += weight;
                            wi1 = weight;
                            Li += throughput * value * bsdf_val * weight;
                        }

                    }
                }


//                if (!bsdf->IsSpecular()) {
//                    float emitter_pdf;
//                    auto emitter = scene->SampleEmitter(emitter_pdf, sampler->Next1D());
//                    EmitterQueryRecord e_rec(its.basic.point);
//                    auto value = emitter->Sample(e_rec, sampler->Next2D());
//                    auto emitter_its = scene->Trace(e_rec.shadow_ray);
//
//                    if (emitter_its.basic.is_hit && emitter_its.hit_type == HitType::Emitter &&
//                        emitter_its.emitter == emitter &&
//                        (emitter_its.basic.point - e_rec.p).norm() <= kEpsilon) {
//
//                        /* Allocate a record for querying the BSDF */
//                        BSDFQueryRecord bRec(its.frame.ToLocal(-ray.dir).normalized(),
//                                             its.frame.ToLocal(-e_rec.wi).normalized(), its.uv);
//
//                        /* Evaluate BSDF * cos(theta) */
//                        Color3f bsdfVal = bsdf->Eval(bRec);
//
//                        /* Prevent light leaks due to the use of shading normals */
//
//                        /* Calculate prob. of having generated that direction
//                           using BSDF sampling */
//                        float bsdfPdf = bsdf->Pdf(bRec);
//                        emitter_pdf *= (e_rec.ref - e_rec.p).squaredNorm();
//                        float dp = abs(e_rec.wi.dot(e_rec.n));
//                        if (dp < kEpsilon)
//                            break;
//
//                        emitter_pdf /= dp;
//
//                        /* Weight using the power heuristic */
//                        float weight = MiWeight(emitter_pdf, bsdfPdf);
//                        //spdlog::info("hit");
//                        Li += throughput * value * bsdfVal * weight / emitter_pdf;
//                    }
//                }

                /* ==================================================================== */
                /*                            BSDF sampling                             */
                /* ==================================================================== */

                /* Sample BSDF * cos(theta) */
                float bsdfPdf;
                BSDFQueryRecord bRec(its.frame.ToLocal(-ray.dir).normalized(), its.uv);
                Color3f bsdfWeight = bsdf->Sample(bRec, bsdfPdf, sampler->Next2D());
                if (bsdfWeight.isZero())
                    break;

//                scattered |= bRec.sampledType != BSDF::ENull;

                /* Prevent light leaks due to the use of shading normals */
                const Vector3f wo = its.frame.ToWorld(bRec.wo).normalized();

                bool hitEmitter = false;
                Color3f value(0.f);

                /* Trace a ray in this direction */
                ray = Ray(its.basic.point, wo);
                its = scene->Trace(ray);
                if (its.basic.is_hit) {
                    /* Intersected something - check if it was a luminaire */
                    if (its.hit_type == HitType::Emitter) {
                        EmitterQueryRecord erec(ray.orig, its.basic.point, its.basic.normal);
                        value = its.emitter->Eval(erec);
                        dRec.SetQuery(ray, its);
                        hitEmitter = true;
                    }
                } else {
                }

                /* Keep track of the throughput and relative
                   refractive index along the path */
                throughput *= bsdfWeight;

                /* If a luminaire was hit, estimate the local illumination and
                   weight using the power heuristic */
                if (hitEmitter) {
                    float emitter_pdf = scene->PdfEmitterDirect(dRec);

                    const float lumPdf = bsdf->IsSpecular() ?
                                         0 : emitter_pdf;
                    wi2 = MiWeight(bsdfPdf, lumPdf);
                    temp += MiWeight(bsdfPdf, lumPdf);
                    Li += throughput * value * MiWeight(bsdfPdf, lumPdf);
                    break;
                }

                /* ==================================================================== */
                /*                         Indirect illumination                        */
                /* ==================================================================== */


                //spdlog::info("{}", temp);

                /* Set the recursive query type. Stop if no surface was hit by the
                   BSDF sample or if indirect illumination was not requested */
                if (!its.basic.is_hit)
                    break;

                if (depth >= mi_recur_) {
                    /* Russian roulette: try to keep path weights equal to one,
                       while accounting for the solid angle compression at refractive
                       index boundaries. Stop with at least some probability to avoid
                       getting stuck (e.g. due to total internal reflection) */

                    if (sampler->Next1D() > russian_)
                        break;
                    throughput /= russian_;
                }
            }

            if (Li.isValid())
                return Li;
            return {0, 0, 0};

//            if (temp < kEpsilon || abs(temp - 1) < kEpsilon)
//                return {0, 0, 0};
            return {temp, wi1, wi2};


        }
    };

    PHOENIX_REGISTER_CLASS(Mis, "mis");

}