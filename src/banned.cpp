// barrystyle 24022020
// banned inputs list
#include "main.h"
typedef std::map<uint256, int> BannedInputs;
const BannedInputs bannedFunds = 
{
    {
	{ uint256S("6673c209fcec7e4f003a3eea9c97d7484bb9a65b956d86cba767c727119e5be8"), 0},       
        { uint256S("1c103b9ee340cb41482054aae4ca5c773637e45b0ddf7442dbb1380b49fdda6e"), 1 },       
        { uint256S("1c103b9ee340cb41482054aae4ca5c773637e45b0ddf7442dbb1380b49fdda6e"), 0 },       
        { uint256S("9d8be33a7dac06ba1b342a2e35468fe10487629cc6d2f0cd4d6d82b81a8f885f"), 0 },      
        { uint256S("8051f1da8140d88809a1c66806020af7cfd136b26b7ba6dcdd2ef7f2a8fc3d37"), 1 },      
    }
};
bool areBannedInputs(uint256 txid, int vout) {
  for (auto inputs : bannedFunds)
    if (inputs.first == txid && inputs.second == vout)
      return true;
  return false;
}
