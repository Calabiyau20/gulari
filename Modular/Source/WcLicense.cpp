#include "WcLicense.h"

namespace wc
{
// The public half of the Calabi Yau signing key: exponent,modulus (1024 bit). Public by design.
static const char* kPublicKey =
    "5,"
    "e2f5265f88cdede26aae3ed4d0c59893ec3ae8b0201c2162b18fc6d732ff5b99ba09a0b84bf46298995d2d1bba692e8bb1ba87900c6658b1f90eb269435de3765d1020cd8e58ecae47f55fcbec0d43600c64cff94a8c834f745e35e29bacd40237a237595f174ddf6eafa959b14a1daa91c5962adc5bfaace00df193fe374555";

static juce::BigInteger hashOf (const juce::String& payload)
{
    const juce::SHA256 sha (payload.toUTF8());
    auto mem = sha.getRawData();
    juce::BigInteger v;
    v.loadFromMemoryBlock (mem);
    return v;
}

static juce::String canonical (const juce::ValueTree& t)
{
    // the exact bytes that are signed: product, version line, buyer, order, date - in this order, nothing else
    return juce::String (License::kProduct) + "\n" + t["ver"].toString() + "\n" + t["name"].toString() + "\n"
           + t["email"].toString() + "\n" + t["order"].toString() + "\n" + t["issued"].toString();
}

juce::String License::summary() const
{
    if (! licensed()) return "Demo mode - no license key found";
    return "Licensed to " + name + (order.isNotEmpty() ? "  (order " + order + ")" : "");
}

juce::String License::sign (const juce::String& payload, const juce::String& privateKey)
{
    juce::RSAKey key (privateKey);
    auto v = hashOf (payload);
    key.applyToValue (v);
    return v.toString (16);
}

juce::String License::makeKeyFile (const juce::String& name, const juce::String& email, const juce::String& order, const juce::String& privateKey)
{
    juce::ValueTree t ("WCLICENSE");
    t.setProperty ("ver", "1", nullptr);
    t.setProperty ("name", name, nullptr);
    t.setProperty ("email", email, nullptr);
    t.setProperty ("order", order, nullptr);
    t.setProperty ("issued", juce::Time::getCurrentTime().toISO8601 (false).substring (0, 8), nullptr);
    const auto sig = sign (canonical (t), privateKey);
    juce::String out;
    out << "# " << kProduct << " - license key\n"
        << "# Keep this file. Put it next to the plugin or in\n"
        << "#   Documents/Calabi Yau/West Coast Modular/" << keyFileName() << "\n"
        << "name="  << name  << "\n"
        << "email=" << email << "\n"
        << "order=" << order << "\n"
        << "issued=" << t["issued"].toString() << "\n"
        << "ver=1\n"
        << "sig=" << sig << "\n";
    return out;
}

License License::validate (const juce::String& blob)
{
    License l;
    if (blob.isEmpty()) return l;
    juce::ValueTree t ("WCLICENSE");
    juce::String sig;
    for (auto& line : juce::StringArray::fromLines (blob))
    {
        const auto s = line.trim();
        if (s.isEmpty() || s.startsWithChar ('#')) continue;
        const auto k = s.upToFirstOccurrenceOf ("=", false, false).trim();
        const auto v = s.fromFirstOccurrenceOf ("=", false, false).trim();
        if (k == "sig") sig = v;
        else if (k == "name" || k == "email" || k == "order" || k == "issued" || k == "ver") t.setProperty (k, v, nullptr);
    }
    if (sig.isEmpty() || ! t.hasProperty ("name") || ! t.hasProperty ("ver")) return l;
    juce::BigInteger v;
    v.parseString (sig, 16);
    if (v.isZero()) return l;
    juce::RSAKey pub (kPublicKey);
    pub.applyToValue (v);
    if (v != hashOf (canonical (t))) return l;
    l.status = Status::Licensed;
    l.name = t["name"].toString();
    l.email = t["email"].toString();
    l.order = t["order"].toString();
    l.issued = t["issued"].toString();
    return l;
}

juce::Array<juce::File> License::searchPaths()
{
    juce::Array<juce::File> f;
    auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("Calabi Yau").getChildFile ("West Coast Modular");
    f.add (docs.getChildFile (keyFileName()));
    f.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
               .getChildFile ("Calabi Yau").getChildFile (keyFileName()));
    f.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Calabi Yau").getChildFile (keyFileName()));
    // beside the plugin binary / bundle
    auto me = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    for (int i = 0; i < 3 && me.exists(); ++i)
    {
        f.add (me.getParentDirectory().getChildFile (keyFileName()));
        me = me.getParentDirectory();
    }
    f.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (keyFileName()));
    return f;
}

License License::findAndValidate (const juce::File& extra)
{
    auto paths = searchPaths();
    if (extra != juce::File()) paths.insert (0, extra);
    for (auto& p : paths)
    {
        if (! p.existsAsFile() || p.getSize() > 16384) continue;
        auto l = validate (p.loadFileAsString());
        if (l.licensed()) return l;
    }
    return {};
}
} // namespace wc
