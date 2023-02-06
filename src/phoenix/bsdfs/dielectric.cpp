#include "phoenix/core/bsdf_class.h"

namespace Phoenix {


    class Dielectric : public Bsdf {

    private:

        [[nodiscard]] Vector3f GetOutgoingVector(const BSDFQueryRecord &bRec, bool returnRefract) const {
            Vector3f n(0, 0, 1);
            float n1 = ext_ior_;
            float n2 = int_ior_;

            float cosT = bRec.wi.z();

            if (cosT < 0) {
                n1 = int_ior_;
                n2 = ext_ior_;
                n = -n;
                cosT = -cosT;
            }

            float snell = n1 / n2;

            Vector3f part1 = -snell * (bRec.wi - cosT * n);
            float cons = sqrt(1.0f - (snell * snell) * (1.0f - cosT * cosT));
            Vector3f part2 = n * cons;

            Vector3f refraction = part1 - part2;
            refraction = refraction.normalized();

            Vector3f reflection = Vector3f(-bRec.wi.x(), -bRec.wi.y(), bRec.wi.z());

            if (returnRefract)
                return refraction;
            else
                return reflection;
        }

        [[nodiscard]] Vector3f GetReflection(const BSDFQueryRecord &bRec) const {
            return GetOutgoingVector(bRec, false);
        }

        [[nodiscard]] Vector3f GetRefraction(const BSDFQueryRecord &bRec) const {
            return GetOutgoingVector(bRec, true);
        }

    private:
        float int_ior_, ext_ior_;
    public:
        explicit Dielectric(PropertyList propers) {
            int_ior_ = 1.5f;
            ext_ior_ = 1.02f;
        }

        [[nodiscard]] string ToString() const override { return "conduct"; }

        void Sample(BSDFQueryRecord &rec, float &pdf, const Vector2f &sample) const override {
            Vector3f n(0, 0, 1);
            float n1 = ext_ior_;
            float n2 = int_ior_;

            float cosT = rec.wi.z();

            if (cosT < 0) {
                n1 = int_ior_;
                n2 = ext_ior_;
                n = -n;
                cosT = -cosT;
            }

            float F = Fresnel(cosT, n1, n2);
            float snell = n1 / n2;

            float cons = sqrt(1.0f - (snell * snell) * (1.0f - cosT * cosT));

            bool isTReflec = cons > 1;

            if (sample.x() < F || isTReflec) {
                rec.wo = GetReflection(rec);


            } else {
                rec.wo = GetRefraction(rec);
            }
        }

        Color3f Eval(const BSDFQueryRecord &rec) const override {
            auto wo = Vector3f(-rec.wi.x(), -rec.wi.y(), rec.wi.z());
            if (rec.wo.isApprox(wo))
                return {1, 1, 1};
            return {0, 0, 0};

        }

        float Pdf(const BSDFQueryRecord &rec) const override {
            auto wo = Vector3f(-rec.wi.x(), -rec.wi.y(), rec.wi.z());
            if (rec.wo.isApprox(wo))
                return 1.f;
            return 0.f;
        }
    };

}