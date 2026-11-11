// Calabi Yau key generator. Never ship this binary or the private key.
//   WcKeygen --newkeys                                   makes a fresh key pair
//   WcKeygen "Buyer Name" buyer@mail.com ORDER-123 <private key> [out.key]
#include "WcLicense.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc >= 2 && juce::String (argv[1]) == "--newkeys")
    {
        juce::RSAKey pub, priv;
        juce::RSAKey::createKeyPair (pub, priv, 1024);
        std::printf ("public  (paste into WcLicense.cpp):\n%s\n\nprivate (keep secret):\n%s\n",
                     pub.toString().toRawUTF8(), priv.toString().toRawUTF8());
        return 0;
    }
    if (argc < 5)
    {
        std::printf ("usage: WcKeygen \"Name\" email order <privatekey> [out.key]\n       WcKeygen --newkeys\n");
        return 1;
    }
    const juce::String name { juce::CharPointer_UTF8 (argv[1]) };
    const juce::String email (argv[2]), order (argv[3]), priv (argv[4]);
    const auto blob = wc::License::makeKeyFile (name, email, order, priv);
    const auto check = wc::License::validate (blob);
    if (! check.licensed()) { std::printf ("FAILED: the key file does not validate (wrong private key?)\n"); return 2; }
    if (argc >= 6) juce::File (juce::String (argv[5])).replaceWithText (blob);
    else std::printf ("%s", blob.toRawUTF8());
    std::printf ("\nOK: %s\n", check.summary().toRawUTF8());
    return 0;
}
