// bootnodes_testnets.h
// Collection of known bootnodes (enode strings) for testnets of major EVM chains
// Format: "enode://pubkey@ip:port"
// Used for discv4 discovery and initial peer connection (RLP/x protocol)
// Last known update reference: early 2026 from official sources/docs
// !!! Always verify against latest official sources before use !!!

#ifndef BOOTNODES_TESTNETS_H
#define BOOTNODES_TESTNETS_H

#include <string>
#include <vector>

// Ethereum Sepolia Testnet – official go-ethereum bootnodes
// Source: https://github.com/ethereum/go-ethereum/blob/master/params/sepolia.go
static const std::vector<std::string> ETHEREUM_SEPOLIA_BOOTNODES = {
    "enode://4e5e92199ee224a01932a377160aa432f31d0b351f84ab413a8e0a42f4f36476f8fb1cbe914af0d9aef0d51665c214cf653c651c4bbd9d5550a934f241f1682b@138.197.51.181:30303",
    "enode://143e11fb766781d22d92a2e33f8f104cddae4411a122295ed1fdb6638de96a6ce65f5b7c964ba3763bba27961738fef7d3ecc739268f3e5e771fb4c87b6234ba@146.190.1.103:30303",
    "enode://8b61dc2d06c3f96fddcbebb0efb29d60d3598650275dc469c22229d3e5620369b0d3dedafd929835fe7f489618f19f456fe7c0df572bf2d914a9f4e006f783a9@170.64.250.88:30303",
    "enode://10d62eff032205fcef19497f35ca8477bea0eadfff6d769a147e895d8b2b8f8ae6341630c645c30f5df6e67547c03494ced3d9c5764e8622a26587b083b028e8@139.59.49.206:30303",
    "enode://9e9492e2e8836114cc75f5b929784f4f46c324ad01daf87d956f98b3b6c5fcba95524d6e5cf9861dc96a2c8a171ea7105bb554a197455058de185fa870970c7c@138.68.123.152:30303"
};

// Ethereum Holesky Testnet – official go-ethereum bootnodes
// Source: https://github.com/ethereum/go-ethereum/blob/master/params/holesky.go
static const std::vector<std::string> ETHEREUM_HOLESKY_BOOTNODES = {
    "enode://ac906289e4b7f12df423d654c5a962b6ebe5b3a74cc9e06292a85221f9a64a6f1cfdd6b714ed6dacef51578f92b34c60ee91e9ede9c7f8fadc4d347326d95e2b@146.190.13.128:30303",
    "enode://a3435a0155a3e837c02f5e7f5662a2f1fbc25b48e4dc232016e1c51b544cb5b4510ef633ea3278c0e970fa8ad8141e2d4d0f9f95456c537ff05fdf9b31c15072@178.128.136.233:30303"
};

// Polygon Amoy Testnet – Bor bootnodes from Polygon docs
// Source: https://docs.polygon.technology/pos/reference/seed-and-bootnodes
static const std::vector<std::string> POLYGON_AMOY_BOOTNODES = {
    "enode://d40ab6b340be9f78179bd1ec7aa4df346d43dc1462d85fb44c5d43f595991d2ec215d7c778a7588906cb4edf175b3df231cecce090986a739678cd3c620bf580@34.89.255.109:30303",
    "enode://13abba15caa024325f2209d3566fa77cd864281dda4f73bca4296277bfd919ac68cef4dbb508028e0310a24f6f9e23c761fa41ac735cdc87efdee76d5ff985a7@34.185.137.160:30303",
    "enode://fc5bd3856a4ce6389eef1d6bc637ce7617e6ba8013f7d722d9878cf13f1c5a5a95a9e26ccb0b38bcc330343941ce117ab50db9f61e72ba450dd528a1184d8e6a@34.89.119.250:30303",
    "enode://945e11d11bdeed301fb23a5c05aae77bfdde39a8f70308131682a5d2fc1f080531314554afc78718a72ae25cc09be7833f760bf8681516b4315ed36217fa8dab@34.89.40.235:30303"
};

// BNB Smart Chain (BSC) Testnet – official bootnodes
// Source: https://github.com/bnb-chain/bsc/blob/master/params/config.go
// Note: BSC testnet uses port 30311 instead of standard 30303
static const std::vector<std::string> BSC_TESTNET_STATICNODES = {
    "enode://1cc4534b14cfe351ab740a1418ab944a234ca2f702915eadb7e558a02010cb7c5a8c295a3b56bcefa7701c07752acd5539cb13df2aab8ae2d98934d712611443@52.71.43.172:30311",
    "enode://28b1d16562dac280dacaaf45d54516b85bc6c994252a9825c5cc4e080d3e53446d05f63ba495ea7d44d6c316b54cd92b245c5c328c37da24605c4a93a0d099c4@34.246.65.14:30311",
    "enode://5a7b996048d1b0a07683a949662c87c09b55247ce774aeee10bb886892e586e3c604564393292e38ef43c023ee9981e1f8b335766ec4f0f256e57f8640b079d5@35.73.137.11:30311"
};

// Base Sepolia Testnet – limited public bootnodes (OP Stack; relies on discv4/discv5 and L1 connection)
// Source: https://docs.base.org/base-chain/node-operators/run-a-base-node
// Most Base/OP Stack nodes use auto-discovery after connecting via L1 RPC
static const std::vector<std::string> BASE_SEPOLIA_BOOTNODES = {
    // Base Sepolia discovery primarily uses OP Stack defaults
    // Check op-geth or op-node repositories for latest discovery nodes
    // https://github.com/base-org/op-geth
};

#endif // BOOTNODES_TESTNETS_H
