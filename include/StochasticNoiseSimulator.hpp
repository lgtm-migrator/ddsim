#ifndef DDSIM_STOCHASTICNOISESIMULATOR_HPP
#define DDSIM_STOCHASTICNOISESIMULATOR_HPP

#include "QuantumComputation.hpp"
#include "Simulator.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

class StochasticNoiseSimulator: public Simulator<StochasticNoiseSimulatorDDPackage> {
public:
    StochasticNoiseSimulator(std::unique_ptr<qc::QuantumComputation>& qc, const unsigned int step_number, const double step_fidelity):
        qc(qc), step_number(step_number), step_fidelity(step_fidelity) {
        dd->resize(qc->getNqubits());
    }

    StochasticNoiseSimulator(std::unique_ptr<qc::QuantumComputation>& qc, const unsigned int step_number, const double step_fidelity, unsigned long long seed):
        Simulator(seed), qc(qc), step_number(step_number), step_fidelity(step_fidelity) {
        dd->resize(qc->getNqubits());
    }

    StochasticNoiseSimulator(std::unique_ptr<qc::QuantumComputation>& qc,
                             const std::string&                       noise_effects,
                             double                                   noise_prob,
                             long                                     stoch_runs,
                             unsigned int                             step_number,
                             double                                   step_fidelity,
                             const std::string&                       recorded_properties):
        qc(qc),
        step_number(step_number), step_fidelity(step_fidelity) {
        setNoiseEffects(noise_effects);
        setRecordedProperties(recorded_properties);
        initializeNoiseProbabilities(noise_prob);
        setNumberOfRuns(stoch_runs);
    }

    std::map<std::string, std::size_t> Simulate(unsigned int shots) override;

    std::map<std::string, double> StochSimulate();

    [[nodiscard]] std::size_t getMaxMatrixNodeCount() const override { return 0; } // Not available for stochastic simulation

    [[nodiscard]] std::size_t getMatrixActiveNodeCount() const override { return 0; } // Not available for stochastic simulation

    [[nodiscard]] std::size_t countNodesFromRoot() const override { return 0; } // Not available for stochastic simulation

    std::map<std::string, std::string> AdditionalStatistics() override {
        return {
                {"step_fidelity", std::to_string(step_fidelity)},
                {"approximation_runs", std::to_string(approximation_runs)},
                //                {"final_fidelity", std::to_string(final_fidelity)},
                {"perfect_run_time", std::to_string(perfect_run_time)},
                {"stoch_wall_time", std::to_string(stoch_run_time)},
                {"mean_stoch_run_time", std::to_string(mean_stoch_time)},
                {"parallel_instances", std::to_string(max_instances)},
        };
    };

    [[nodiscard]] dd::QubitCount getNumberOfQubits() const override { return qc->getNqubits(); };

    [[nodiscard]] std::size_t getNumberOfOps() const override { return qc->getNops(); };

    [[nodiscard]] std::string getName() const override { return "stoch_" + gateNoiseEffects + "_" + qc->getName(); };

    double           noiseProbability = 0.0;
    dd::ComplexValue sqrtAmplitudeDampingProbability{};
    dd::ComplexValue oneMinusSqrtAmplitudeDampingProbability{};

    double           noiseProbabilityMulti = 0.0;
    dd::ComplexValue sqrtAmplitudeDampingProbabilityMulti{};
    dd::ComplexValue oneMinusSqrtAmplitudeDampingProbabilityMulti{};

    dd::GateMatrix ampDampingTrue      = {};
    dd::GateMatrix ampDampingTrueMulti = {};

    dd::GateMatrix ampDampingFalse      = {};
    dd::GateMatrix ampDampingFalseMulti = {};

    void setNumberOfRuns(long stoch_run) {
        if (stoch_run <= 0) {
            throw std::runtime_error("Number of stochastic runs must be larger than 0. Provided value: " + std::to_string(stoch_run));
        }
        stochasticRuns = stoch_run;
    }

    void initializeNoiseProbabilities(double cGateNoiseProbability, double amplitudeDampingProb = -1, double multiQubitGateFactor = 2) {
        if (amplitudeDampingProb < 0) {
            // Default value for amplitude damping prob is double the general error probability
            amplitudeDampingProb = cGateNoiseProbability * 2;
        }
        //The probability of amplitude damping (t1) often is double the probability , of phase flip, which is why I double it here
        noiseProbability                        = cGateNoiseProbability;
        sqrtAmplitudeDampingProbability         = {sqrt(amplitudeDampingProb), 0};
        oneMinusSqrtAmplitudeDampingProbability = {sqrt(1 - amplitudeDampingProb), 0};

        noiseProbabilityMulti                        = cGateNoiseProbability * multiQubitGateFactor;
        sqrtAmplitudeDampingProbabilityMulti         = {sqrt(noiseProbability) * multiQubitGateFactor, 0};
        oneMinusSqrtAmplitudeDampingProbabilityMulti = {sqrt(1 - multiQubitGateFactor * amplitudeDampingProb), 0};
        ampDampingFalse                              = dd::GateMatrix({dd::complex_one, dd::complex_zero, dd::complex_zero, oneMinusSqrtAmplitudeDampingProbability});
        ampDampingFalseMulti                         = dd::GateMatrix({dd::complex_one, dd::complex_zero, dd::complex_zero, oneMinusSqrtAmplitudeDampingProbabilityMulti});

        ampDampingTrue      = dd::GateMatrix({dd::complex_zero, sqrtAmplitudeDampingProbability, dd::complex_zero, dd::complex_zero});
        ampDampingTrueMulti = dd::GateMatrix({dd::complex_zero, sqrtAmplitudeDampingProbabilityMulti, dd::complex_zero, dd::complex_zero});
        if (amplitudeDampingProb * multiQubitGateFactor > 1 || noiseProbability < 0) {
            throw std::runtime_error("Error probabilities are faulty!"
                                     "\n single qubit error probability: " +
                                     std::to_string(cGateNoiseProbability) +
                                     " multi qubit error probability: " + std::to_string(cGateNoiseProbability * multiQubitGateFactor) +
                                     "\n single qubit amplitude damping  probability: " + std::to_string(amplitudeDampingProb) +
                                     " multi qubit amplitude damping  probability: " + std::to_string(amplitudeDampingProb * multiQubitGateFactor));
        }
    }

    unsigned int stochasticRuns       = 0;
    bool         sequentialApplyNoise = false;

    void setRecordedProperties(const std::string& input);

    std::vector<std::tuple<long, std::string>> recorded_properties;
    std::vector<std::vector<double>>           recorded_properties_per_instance;
    std::vector<std::map<std::string, int>>    classical_measurements_maps;

    std::string gateNoiseEffects;

    void setNoiseEffects(const std::string& cGateNoise) {
        for (const auto& effect: cGateNoise) {
            if (effect != 'A' && effect != 'P' && effect != 'D') {
                throw std::runtime_error("Unknown noise operation '" + std::to_string(effect) + "'\n");
            }
        }
        gateNoiseEffects = cGateNoise;
    }

    const unsigned int max_instances = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 4);
    //    const unsigned int max_instances = 1; // use for debugging only

private:
    std::unique_ptr<qc::QuantumComputation>& qc;

    const unsigned int step_number;
    const double       step_fidelity;
    double             approximation_runs{0};
    long double        final_fidelity{1.0L};

    float  perfect_run_time{0};
    float  stoch_run_time{0};
    double mean_stoch_time{0};

    void perfect_simulation_run();

    void runStochSimulationForId(unsigned int                                stochRun,
                                 int                                         nQubits,
                                 std::vector<double>&                        recordedPropertiesStorage,
                                 std::vector<std::tuple<long, std::string>>& recordedPropertiesList,
                                 std::map<std::string, int>&                 classicalMeasurementsMap,
                                 unsigned long long                          localSeed);

    dd::mEdge generateNoiseOperation(const std::unique_ptr<StochasticNoiseSimulatorDDPackage>& localDD,
                                     dd::mEdge                                                 dd_operation,
                                     signed char                                               target,
                                     std::string&                                              noiseOperation,
                                     std::mt19937_64&                                          generator,
                                     std::uniform_real_distribution<dd::fp>&                   distribution,
                                     bool                                                      amplitudeDamping,
                                     bool                                                      multiQubitOperation);

    void applyNoiseOperation(const std::vector<dd::Qubit>&                             usedQubits,
                             dd::mEdge                                                 dd_op,
                             const std::unique_ptr<StochasticNoiseSimulatorDDPackage>& localDD,
                             dd::vEdge&                                                localRootEdge,
                             std::mt19937_64&                                          generator,
                             std::uniform_real_distribution<dd::fp>&                   dist,
                             const dd::mEdge&                                          identityDD,
                             std::string&                                              noiseOperation);

    [[nodiscard]] qc::OpType ReturnNoiseOperation(char i, double prob, bool multi_qubit_noise) const;

    [[nodiscard]] std::string intToString(long target_number) const;

    //    double ApproximateEdgeByFidelity(std::unique_ptr<dd::Package>& localDD, dd::Package::vEdge& edge, double targetFidelity, bool allLevels, bool removeNodes);
    //
    //    dd::Package::vEdge RemoveNodesInPackage(std::unique_ptr<dd::Package>& localDD, dd::Package::vEdge e, std::map<dd::Package::vNode*, dd::Package::vEdge>& dag_edges);
    void setMeasuredQubitToZero(signed char& at, dd::vEdge& e, std::unique_ptr<StochasticNoiseSimulatorDDPackage>& localDD);
};

#endif //DDSIM_STOCHASTICNOISESIMULATOR_HPP
