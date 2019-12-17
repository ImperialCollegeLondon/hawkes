#ifndef _NEWHAWKES_HPP
#define _NEWHAWKES_HPP

#include <numeric>
#include <vector>

#define TBB_PREVIEW_GLOBAL_CONTROL 1
#include "tbb/global_control.h"

#ifdef RBUILD
#include <Rcpp.h>
#else
#include <iostream>
#endif


//#define XSIMD_ENABLE_FALLBACK

#include "xsimd/xsimd.hpp"
#include "AbstractHawkes.hpp"
#include "Distance.hpp"

namespace hph {

    struct DefaultOut {

        template <typename T>
        DefaultOut& operator<<(const T& val) {
#ifdef RBUILD
            Rcpp::Rcout << val;
#else
            std::cout << val;
#endif
            return *this;
        }

        DefaultOut& operator<<(std::ostream&(*pManip)(std::ostream&)) {
#ifdef RBUILD
            Rcpp::Rcout << (*pManip);
#else
            std::cout << (*pManip);
#endif
            return *this;
        }
    };

    static DefaultOut defaultOut;

	struct DoubleNoSimdTypeInfo {
		using BaseType = double;
		using SimdType = double;
		static const int SimdSize = 1;
	};

    struct FloatNoSimdTypeInfo {
        using BaseType = float;
        using SimdType = float;
        static const int SimdSize = 1;
    };

#ifdef USE_SIMD

#ifdef USE_SSE
    struct DoubleSseTypeInfo {
        using BaseType = double;
        using SimdType = xsimd::batch<double, 2>;
        static const int SimdSize = 2;
    };

    struct FloatSseTypeInfo {
        using BaseType = float;
        using SimdType = xsimd::batch<float, 4>;
        static const int SimdSize = 4;
    };
#endif

#ifdef USE_AVX
    struct DoubleAvxTypeInfo {
        using BaseType = double;
        using SimdType = xsimd::batch<double, 4>;
        static const int SimdSize = 4;
    };
#endif

#ifdef USE_AVX512
    struct DoubleAvx512TypeInfo {
        using BaseType = double;
        using SimdType = xsimd::batch<double, 8>;
        static const int SimdSize = 8;
    };
#endif

#endif

template <typename TypeInfo, typename ParallelType>
class NewHawkes : public AbstractHawkes {
public:

	using RealType = typename TypeInfo::BaseType;

    NewHawkes(int embeddingDimension, int locationCount, long flags, int threads)
        : AbstractHawkes(embeddingDimension, locationCount, flags),
          sigmaXprec(0.0), storedSigmaXprec(0.0),
          tauXprec(0.0), storedTauXprec(0.0),
          tauTprec(0.0), storedTauTprec(0.0),
          omega(0.0), storedOmega(0.0),
          theta(0.0), storedTheta(0.0),
          mu0(0.0), storedMu0(0.0),

          locDists(locationCount * locationCount),
          timDiffs(locationCount * locationCount),

          times(locationCount),

          locations0(locationCount * embeddingDimension),
          locations1(locationCount * embeddingDimension),
          locationsPtr(&locations0),
          storedLocationsPtr(&locations1),

          sumOfLikContribs(0.0), storedSumOfLikContribs(0.0),

          likContribs(locationCount),
          storedLikContribs(locationCount),

          gradientPtr(&gradient),
          ratesVectorPtr(&ratesVector),

          isStoredLikContribsEmpty(false),

          nThreads(threads)
    {

#ifdef USE_TBB
        if (flags & hph::Flags::TBB) {
    		if (nThreads <= 0) {
                nThreads = tbb::task_scheduler_init::default_num_threads();
    		}

    		defaultOut << "Using " << nThreads << " threads" << std::endl;

            control = std::make_shared<tbb::global_control>(tbb::global_control::max_allowed_parallelism, nThreads);
    	}
#endif
    }



    virtual ~NewHawkes() { }

	int getInternalDimension() { return embeddingDimension; }

    void updateLocations(int locationIndex, double* location, size_t length) {

        size_t offset{0};

        if (locationIndex == -1) {
            // Update all locations
            assert(length == embeddingDimension * locationCount);

//            incrementsKnown = false;
//            isStoredIncrementsEmpty = true;

            // TODO Do anything with updatedLocation?
        } else {
            // Update a single location
            assert(length == embeddingDimension);

            if (updatedLocation != - 1) {
                // more than one location updated -- do a full recomputation
//                incrementsKnown = false;
//                isStoredIncrementsEmpty = true;
            }

            updatedLocation = locationIndex;
            offset = locationIndex * embeddingDimension;
        }

        mm::bufferedCopy(location, location + length,
                         begin(*locationsPtr) + offset,
                         buffer
        );

//        sumOfIncrementsKnown = false;
    }

    double getSumOfLikContribs() {
        // TODO do lazy computation (i.e., check if changed)
        sumOfLikContribs = computeSumOfLikContribsGeneric<typename TypeInfo::SimdType, TypeInfo::SimdSize, Generic>();
    	return sumOfLikContribs;
 	}

    void storeState() {
    	storedSumOfLikContribs = sumOfLikContribs;
        sigmaXprec = storedSigmaXprec;
        tauXprec = storedTauXprec;
        tauTprec = storedTauTprec;
        omega = storedOmega;
        theta = storedTheta;
        mu0 = storedMu0;

        std::copy(begin(*locationsPtr), end(*locationsPtr),
                  begin(*storedLocationsPtr));
    }

    void acceptState() {
        if (!isStoredLikContribsEmpty) {
    		for (int j = 0; j < locationCount; ++j) {
    			likContribs[j * locationCount + updatedLocation] = likContribs[updatedLocation * locationCount + j];
    		}
    	}
    }

    void restoreState() {
    	sumOfLikContribs = storedSumOfLikContribs;

        sigmaXprec = storedSigmaXprec;
        tauXprec = storedTauXprec;
        tauTprec = storedTauTprec;
        omega = storedOmega;
        theta = storedTheta;
        mu0 = storedMu0;

        auto tmp1 = storedLocationsPtr;
        storedLocationsPtr = locationsPtr;
        locationsPtr = tmp1;
    }



    // TODO This could be very problematic when/if some/all times are random
    // Then library should (1) lazy-compute time-differences and (2) return gradient w.r.t. times
    void setTimDiffsData(double* data, size_t length) {
        assert(length == timDiffs.size());
        mm::bufferedCopy(data, data + length, begin(timDiffs), buffer);
    }

    void setTimesData(double* data, size_t length) {
        assert(length == times.size());
        mm::bufferedCopy(data, data + length, begin(times), buffer);
    }

    void setParameters(double* data, size_t length) {
        assert(length == 6);
        sigmaXprec = data[0];
        tauXprec = data[1];
        tauTprec = data[2];
        omega = data[3];
        theta = data[4];
        mu0 = data[5];
    }

	void getLogLikelihoodGradient(double* result, size_t length) {
		assert (length == 6);
		computeLogLikelihoodGradientGeneric<typename TypeInfo::SimdType, TypeInfo::SimdSize, Generic>();
		mm::bufferedCopy(std::begin(*gradientPtr), std::end(*gradientPtr), result, buffer);
    }

	template <typename SimdType, int SimdSize, typename Algorithm>
    RealType computeLogLikelihoodGradientGeneric() {

        const auto length = 6;
        if (length != gradientPtr->size()) {
            gradientPtr->resize(length);
        }

        const auto sigmaXprecD = pow(sigmaXprec, embeddingDimension);
        const auto tauXprecD = pow(tauXprec, embeddingDimension);
        const auto tauTprec2 = tauTprec * tauTprec;

        const auto grad =
                accumulate(0, locationCount, RealTypePack<7>(0.0), [this,
                                                                    sigmaXprecD,
                                                                    tauXprecD,
                                                                    tauTprec2](const int i) {

                    const int vectorCount = locationCount - locationCount % SimdSize;

                    DistanceDispatch<SimdType, RealType, Algorithm> dispatch(*locationsPtr, i, embeddingDimension);
                    auto sumOfRates = innerLoop1<SimdType, SimdSize, 7>(dispatch, i, 0, vectorCount,
                            sigmaXprecD, tauXprecD, tauTprec2);

                    if (vectorCount < locationCount) { // Edge-cases
                        DistanceDispatch<RealType, RealType, Algorithm> dispatch(*locationsPtr, i, embeddingDimension);
                        sumOfRates += innerLoop1<RealType, 1, 7>(dispatch, i, vectorCount, locationCount,
                                sigmaXprecD, tauXprecD, tauTprec2);
                    }

                    auto const timDiff = times[locationCount-1]-times[i];
                    auto const expOmegaTimDiff = xsimd::exp(-omega*timDiff);

                    sumOfRates[0] /= sumOfRates[6];
                    sumOfRates[1] /= sumOfRates[6];
                    sumOfRates[2] = sumOfRates[2]/sumOfRates[6] * tauXprecD +
                            math::pdf_new(tauTprec * timDiff) * timDiff + math::pdf_new(tauTprec*times[i])*times[i];
                    sumOfRates[3] = (1-(1+omega*timDiff) * expOmegaTimDiff)/(omega*omega) - sumOfRates[3]/sumOfRates[6] * sigmaXprecD;
                    sumOfRates[4] = sumOfRates[4]/sumOfRates[6] * sigmaXprecD + (expOmegaTimDiff-1)/omega;
                    sumOfRates[5] = sumOfRates[5]/sumOfRates[6] * tauXprecD * tauTprec -
                            (xsimd::exp(math::phi_new(tauTprec*timDiff)) - xsimd::exp(math::phi_new(tauTprec*(-times[i]))));
                    sumOfRates[6] = std::log(sumOfRates[6]);

                    return sumOfRates;

                }, ParallelType());

        (*gradientPtr)[0] = grad[0] * theta * sigmaXprecD * sigmaXprec;        //sigmaX
        (*gradientPtr)[1] = grad[1] * mu0 * tauXprecD * tauXprec * tauTprec;   //tauX
        (*gradientPtr)[2] = grad[2] * mu0 * tauTprec2;                         //tauT
        (*gradientPtr)[3] = grad[3] * theta;                                   //omega
        (*gradientPtr)[4] = grad[4];                                           //theta
        (*gradientPtr)[5] = grad[5];                                           //mu0

        return grad[6]; // TODO log-likelihood
    }

#ifdef USE_SIMD

	template <typename T, size_t N>
	using SimdBatch = xsimd::batch<T, N>;

	template <typename T, size_t N>
	using SimdBatchBool = xsimd::batch_bool<T, N>;

	template <typename T, size_t N>
	bool any(SimdBatchBool<T, N> x) {
	    return xsimd::any(x);
	}

	template <typename T, size_t N>
	T reduce(SimdBatch<T, N> x) {
	    return xsimd::hadd(x);
	}

	template <typename T, size_t N>
	SimdBatch<T, N> mask(SimdBatchBool<T, N> flag, SimdBatch<T, N> x) {
	    return SimdBatch<T, N>(flag()) & x;
	}

	template <typename T, size_t N>
	SimdBatchBool<T, N> getMissing(int i, int j, SimdBatch<T, N> x) {
        return SimdBatch<T, N>(i) == (SimdBatch<T, N>(j) + getIota(SimdBatch<T, N>())) || xsimd::isnan(x);
	}
#endif

#ifdef USE_SSE
	using D2 = xsimd::batch<double, 2>;
	using D2Bool = xsimd::batch_bool<double, 2>;
	using S4 = xsimd::batch<float, 4>;
	using S4Bool = xsimd::batch_bool<float, 4>;

    D2 getIota(D2) {
        return D2(0, 1);
    }

    S4 getIota(S4) {
        return S4(0, 1, 2, 3);
    }
#endif

#ifdef USE_AVX
	using D4 = xsimd::batch<double, 4>;
	using D4Bool = xsimd::batch_bool<double, 4>;

	D4 getIota(D4) {
        return D4(0, 1, 2, 3);
    }
#endif

#ifdef USE_AVX512
    using D8 = xsimd::batch<double, 8>;
	using D8Bool = xsimd::batch_bool<double, 8>;

	D8 getIota(D8) {
	    return D8(0, 1, 2, 3, 4, 5, 6, 7);
	}

	D8 mask(D8Bool flag, D8 x) {
		return xsimd::select(flag, x, D8(0.0)); // bitwise & does not appear to work
    }
#endif

    template <typename T>
    bool getMissing(int i, int j, T x) {
        return i == j || std::isnan(x);
    }

    template <typename T>
    T mask(bool flag, T x) {
        return flag ? x : T(0.0);
    }

    template <typename T>
    T reduce(T x) {
        return x;
    }

	bool any(bool x) {
		return x;
	}

	double getScalar(double x, int i) {
		return x;
	}

	float getScalar(float x, int i) {
		return x;
	}

#ifdef USE_SIMD
#ifdef USE_AVX
	double getScalar(D4 x, int i) {
		return x[i];
	}
#endif
#ifdef USE_SSE
	double getScalar(D2 x, int i) {
		return x[i];
	}

	float getScalar(S4 x, int i) {
		return x[i];
	}
#endif
#endif // USE_SIMD

#ifdef USE_AVX512
    double getScalar(D8 x, int i) {
	    return x[i];
	}
#endif

    template <typename SimdType, int SimdSize, typename DispatchType>
    RealType ratesLoop(const DispatchType& dispatch, const int i, const int begin, const int end) {

        auto sum = SimdType(RealType(0));
        const auto zero = SimdType(RealType(0));

        const auto tauXprecD = pow(tauXprec, embeddingDimension);
        const auto sigmaXprecD = pow(sigmaXprec, embeddingDimension);
        const auto mu0TauXprecDTauTprec = mu0 * tauXprecD * tauTprec;
        const auto sigmaXprecDTheta = sigmaXprecD * theta;

        const auto timeI = SimdType(RealType(times[i]));

        for (int j = begin; j < end; j += SimdSize) {

            const auto locDist = dispatch.calculate(j); //SimdHelper<SimdType, RealType>::get(&locDists[i * locationCount + j]);
            const auto timDiff = timeI - SimdHelper<SimdType, RealType>::get(&times[j]);

            const auto rate =  mu0TauXprecDTauTprec *
                    math::pdf_new(locDist * tauXprec) * math::pdf_new(timDiff * tauTprec) +
                    sigmaXprecDTheta *mask(timDiff > zero,
                         xsimd::exp(-omega * timDiff) * math::pdf_new(locDist * sigmaXprec));

            sum += rate;
        }

        return reduce(sum);
    }

    template <int N>
	class RealTypePack {
	public:
	    RealTypePack(RealType x) {
	        pack.fill(x);
	    }

	    RealTypePack& operator+=(const RealTypePack& rhs) {
	        for (int i = 0; i < N; ++i) {
	            pack[i] += rhs[i];
	        }
	        return *this;
	    }

	    const RealTypePack operator+(const RealTypePack& rhs) const {
	        RealTypePack result = *this;
	        result += rhs;
	        return result;
	    }

	    RealType& operator[](std::size_t i) {
	        return pack[i];
	    }

	    const RealType& operator[](std::size_t i) const {
	        return pack[i];
	    }

	private:
	    std::array<RealType, N> pack;
	};

	template <typename SimdType, int N>
	RealTypePack<N> reduce(const std::array<SimdType, N> rhs) {

	    RealTypePack<N> pack(0.0);
	    for (int i = 0; i < N; ++i) {
	        pack[i] = reduce(rhs[i]);
	    }

	    return pack;
	}

    template <typename SimdType, int SimdSize, int N, typename DispatchType>
    RealTypePack<N> innerLoop1(const DispatchType& dispatch, const int i, const int begin, const int end,
            const RealType sigmaXprecD, const RealType tauXprecD, const RealType tauTprec2) {

        const auto sigmaXprec2 = sigmaXprec * sigmaXprec;
        const auto tauXprec2 = tauXprec * tauXprec;
        const auto mu0TauXprecDTauTprec = mu0 * tauXprecD * tauTprec;
        const auto sigmaXprecDTheta = sigmaXprecD * theta;

		const auto zero = SimdType(RealType(0));
		std::array<SimdType, N> sum = {zero, zero, zero, zero, zero, zero, zero};

        const auto timeI = SimdType(RealType(times[i]));

        for (int j = begin; j < end; j += SimdSize) {
            const auto locDist = dispatch.calculate(j);//SimdHelper<SimdType, RealType>::get(&locDists[i * locationCount + j]);
            const auto timDiff = timeI - SimdHelper<SimdType, RealType>::get(&times[j]);

            const auto pdfLocDistSigmaXPrec = math::pdf_new(locDist * sigmaXprec);
            const auto pdfLocDistTauXPrec = math::pdf_new(locDist * tauXprec);
            const auto pdfTimDiffTauTPrec = math::pdf_new(timDiff * tauTprec);
            const auto expOmegaTimDiff = xsimd::exp(-omega*timDiff);

            const auto mu0Rate = pdfLocDistTauXPrec * pdfTimDiffTauTPrec;
            const auto thetaRate = mask(timDiff > zero, expOmegaTimDiff * pdfLocDistSigmaXPrec);

            const auto sigmaXrate = (sigmaXprec2*locDist*locDist - embeddingDimension) * thetaRate;
            const auto tauXrate = (tauXprec2 * locDist * locDist - embeddingDimension) * mu0Rate;
            const auto tauTrate = (tauTprec2 * timDiff * timDiff - 1) * mu0Rate;
            const auto omegaRate = timDiff * thetaRate;
            const auto totalRate = mu0TauXprecDTauTprec * mu0Rate + sigmaXprecDTheta * thetaRate;

            sum[0] += sigmaXrate;
            sum[1] += tauXrate;
            sum[2] += tauTrate;
            sum[3] += omegaRate;
            sum[4] += thetaRate;
            sum[5] += mu0Rate;
            sum[6] += totalRate;
	    }

        return reduce<SimdType,N>(sum);
	}

    template <typename SimdType, int SimdSize, typename Algorithm>
    RealType computeSumOfLikContribsGeneric() {

        RealType delta =
                accumulate(0, locationCount, RealType(0), [this](const int i) {

                    const int vectorCount = locationCount - locationCount % SimdSize;

                    DistanceDispatch<SimdType, RealType, Algorithm> dispatch(*locationsPtr, i, embeddingDimension);
					RealType sumOfRates = ratesLoop<SimdType, SimdSize>(dispatch, i, 0, vectorCount);

                    if (vectorCount < locationCount) { // Edge-cases
                        DistanceDispatch<RealType, RealType, Algorithm> dispatch(*locationsPtr, i, embeddingDimension);
                        sumOfRates += ratesLoop<RealType, 1>(dispatch, i, vectorCount, locationCount);
                    }

                    return xsimd::log(sumOfRates) +
                           theta / omega * (xsimd::exp(-omega * (times[locationCount - 1] - times[i])) - 1) -
                           mu0 * (xsimd::exp(math::phi_new(tauTprec * (times[locationCount - 1] - times[i]))) -
                                xsimd::exp(math::phi_new(tauTprec * (-times[i]))));

                }, ParallelType());

        return delta + locationCount * (embeddingDimension - 1) * log(M_1_SQRT_2PI);
    }

// Parallelization helper functions

	template <typename Integer, typename Function>
	inline void for_each(Integer begin, Integer end, Function function, CpuAccumulate) {
	    for (; begin != end; ++begin) {
	        function(begin);
	    }
	}

	template <typename Integer, typename Function, typename Real>
	inline Real accumulate(Integer begin, Integer end, Real sum, Function function, CpuAccumulate) {
		for (; begin != end; ++begin) {
			sum += function(begin);
		}
		return sum;
	}

#ifdef USE_C_ASYNC
	template <typename Integer, typename Function, typename Real>
	inline Real accumulate_thread(Integer begin, Integer end, Real sum, Function function) {
		std::vector<std::future<Real>> results;

		int chunkSize = (end - begin) / nThreads;
		int start = 0;

		for (int i = 0; i < nThreads - 1; ++i, start += chunkSize) {
			results.emplace_back(std::async([=] {
				return accumulate(begin + start, begin + start + chunkSize, 0.0, function);
			}));
		}
		results.emplace_back(std::async([=] {
			return accumulate(begin + start, end, 0.0, function);
		}));

		for (auto&& result : results) {
			sum += result.get();
		}
		return sum;
	}
#endif

#ifdef USE_OMOP
	template <typename Integer, typename Function, typename Real>
	inline Real accumulate_omp(Integer begin, Integer end, Real sum, Function function) {
		#pragma omp
		for (; begin != end; ++begin) {
			sum += function(begin);
		}
		return sum;
	}
#endif

#ifdef USE_THREAD_POOL
	template <typename Integer, typename Function, typename Real>
	inline Real accumulate_thread_pool(Integer begin, Integer end, Real sum, Function function) {
		std::vector<std::future<Real>> results;

		int chunkSize = (end - begin) / nThreads;
		int start = 0;

		for (int i = 0; i < nThreads - 1; ++i, start += chunkSize) {
			results.emplace_back(
				pool.enqueue([=] {
					return accumulate(
						begin + start,
						begin + start + chunkSize,
						Real(0),
						function);
				})
			);
		}
		results.emplace_back(
			pool.enqueue([=] {
				return accumulate(
					begin + start,
					end,
					Real(0),
					function);
			})

		);

		Real total = static_cast<Real>(0);
		for (auto&& result : results) {
			total += result.get();
		}
		return total;
	}
#endif

#ifdef USE_TBB
	template <typename Integer, typename Function, typename Real>
	inline Real accumulate(Integer begin, Integer end, Real sum, Function function, TbbAccumulate) {

		return tbb::parallel_reduce(
 			tbb::blocked_range<size_t>(begin, end
 			//, 200
 			),
 			sum,
 			[function](const tbb::blocked_range<size_t>& r, Real sum) -> Real {
 				//return accumulate
 				const auto end = r.end();
 				for (auto i = r.begin(); i != end; ++i) {
 					sum += function(i);
 				}
 				return sum;
 			},
 			std::plus<Real>()
		);
	}

	template <typename Integer, typename Function>
	inline void for_each(Integer begin, const Integer end, Function function, TbbAccumulate) {

		tbb::parallel_for(
				tbb::blocked_range<size_t>(begin, end
				//, 200
				),
				[function](const tbb::blocked_range<size_t>& r) -> void {
					const auto end = r.end();
					for (auto i = r.begin(); i != end; ++i) {
						function(i);
					}
				}
		);
	}
#endif

private:
    double sigmaXprec;
    double storedSigmaXprec;
    double tauXprec;
    double storedTauXprec;
    double tauTprec;
    double storedTauTprec;
    double omega;
    double storedOmega;
    double theta;
    double storedTheta;
    double mu0;
    double storedMu0;

    double sumOfLikContribs;
    double storedSumOfLikContribs;

    mm::MemoryManager<RealType> locDists;
    mm::MemoryManager<RealType> timDiffs;

    mm::MemoryManager<RealType> times;

    mm::MemoryManager<RealType> locations0;
    mm::MemoryManager<RealType> locations1;

    mm::MemoryManager<RealType>* locationsPtr;
    mm::MemoryManager<RealType>* storedLocationsPtr;


    mm::MemoryManager<RealType> likContribs;
    mm::MemoryManager<RealType> storedLikContribs;

    mm::MemoryManager<RealType> ratesVector;
    mm::MemoryManager<RealType>* ratesVectorPtr;

    mm::MemoryManager<RealType> gradient;
    mm::MemoryManager<RealType>* gradientPtr;

    mm::MemoryManager<double> buffer;

    bool isStoredLikContribsEmpty;

    int nThreads;

#ifdef USE_TBB
    std::shared_ptr<tbb::global_control> control;
#endif

};

// factory
std::shared_ptr<AbstractHawkes>
constructNewHawkesDoubleNoParallelNoSimd(int embeddingDimension, int locationCount, long flags, int threads) {
	defaultOut << "DOUBLE, NO PARALLEL, NO SIMD" << std::endl;
	return std::make_shared<NewHawkes<DoubleNoSimdTypeInfo, CpuAccumulate>>(embeddingDimension, locationCount, flags, threads);
}

std::shared_ptr<AbstractHawkes>
constructNewHawkesDoubleTbbNoSimd(int embeddingDimension, int locationCount, long flags, int threads) {
    defaultOut << "DOUBLE, TBB PARALLEL, NO SIMD" << std::endl;
    return std::make_shared<NewHawkes<DoubleNoSimdTypeInfo, TbbAccumulate>>(embeddingDimension, locationCount, flags, threads);
}

std::shared_ptr<AbstractHawkes>
constructNewHawkesFloatNoParallelNoSimd(int embeddingDimension, int locationCount, long flags, int threads) {
    defaultOut << "SINGLE, NO PARALLEL, NO SIMD" << std::endl;
    return std::make_shared<NewHawkes<FloatNoSimdTypeInfo, CpuAccumulate>>(embeddingDimension, locationCount, flags, threads);
}

std::shared_ptr<AbstractHawkes>
constructNewHawkesFloatTbbNoSimd(int embeddingDimension, int locationCount, long flags, int threads) {
    defaultOut << "SINGLE, TBB PARALLEL, NO SIMD" << std::endl;
    return std::make_shared<NewHawkes<FloatNoSimdTypeInfo, TbbAccumulate>>(embeddingDimension, locationCount, flags, threads);
}

#ifdef USE_SIMD

#ifdef USE_AVX
    std::shared_ptr<AbstractHawkes>
    constructNewHawkesDoubleNoParallelAvx(int embeddingDimension, int locationCount, long flags, int threads) {
        defaultOut << "DOUBLE, NO PARALLEL, AVX" << std::endl;
        return std::make_shared<NewHawkes<DoubleAvxTypeInfo, CpuAccumulate>>(embeddingDimension, locationCount, flags, threads);
    }

    std::shared_ptr<AbstractHawkes>
    constructNewHawkesDoubleTbbAvx(int embeddingDimension, int locationCount, long flags, int threads) {
        defaultOut << "DOUBLE, TBB PARALLEL, AVX" << std::endl;
        return std::make_shared<NewHawkes<DoubleAvxTypeInfo, TbbAccumulate>>(embeddingDimension, locationCount, flags, threads);
    }
#endif

#ifdef USE_AVX512
    std::shared_ptr<AbstractHawkes>
    constructNewHawkesDoubleNoParallelAvx512(int embeddingDimension, int locationCount, long flags, int threads) {
        defaultOut << "DOUBLE, NO PARALLEL, AVX512" << std::endl;
        return std::make_shared<NewHawkes<DoubleAvx512TypeInfo, CpuAccumulate>>(embeddingDimension, locationCount, flags, threads);
    }

    std::shared_ptr<AbstractHawkes>
    constructNewHawkesDoubleTbbAvx512(int embeddingDimension, int locationCount, long flags, int threads) {
        defaultOut << "DOUBLE, TBB PARALLEL, AVX512" << std::endl;
        return std::make_shared<NewHawkes<DoubleAvx512TypeInfo, TbbAccumulate>>(embeddingDimension, locationCount, flags, threads);
    }
#endif

#ifdef USE_SSE
    std::shared_ptr<AbstractHawkes>
    constructNewHawkesDoubleNoParallelSse(int embeddingDimension, int locationCount, long flags, int threads) {
        defaultOut << "DOUBLE, NO PARALLEL, SSE" << std::endl;
        return std::make_shared<NewHawkes<DoubleSseTypeInfo, CpuAccumulate>>(embeddingDimension, locationCount, flags, threads);
    }

    std::shared_ptr<AbstractHawkes>
    constructNewHawkesDoubleTbbSse(int embeddingDimension, int locationCount, long flags, int threads) {
        defaultOut << "DOUBLE, TBB PARALLEL, SSE" << std::endl;
        return std::make_shared<NewHawkes<DoubleSseTypeInfo, TbbAccumulate>>(embeddingDimension, locationCount, flags, threads);
    }

	std::shared_ptr<AbstractHawkes>
	constructNewHawkesFloatNoParallelSse(int embeddingDimension, int locationCount, long flags, int threads) {
        defaultOut << "SINGLE, NO PARALLEL, SSE" << std::endl;
        return std::make_shared<NewHawkes<FloatSseTypeInfo, CpuAccumulate>>(embeddingDimension, locationCount, flags, threads);
	}

	std::shared_ptr<AbstractHawkes>
	constructNewHawkesFloatTbbSse(int embeddingDimension, int locationCount, long flags, int threads) {
        defaultOut << "SINGLE, TBB PARALLEL, SSE" << std::endl;
        return std::make_shared<NewHawkes<FloatSseTypeInfo, TbbAccumulate>>(embeddingDimension, locationCount, flags, threads);
	}
#endif

#endif

} // namespace hph

#endif // _NEWHAWKES_HPP
