#include "parameter_definitions.h"

namespace {
constexpr ParameterDefinition kParameterDefinitions[] = {
    {"U1", "Volume stap 1", "Stelt de luchthoeveelheid voor ventilatiestand 1 in.", "50..(max-10)", "100"},
    {"U2", "Volume stap 2", "Stelt de luchthoeveelheid voor ventilatiestand 2 in.", "50..(max-5)", "Medium 150 / Large 200"},
    {"U3", "Volume stap 3", "Stelt de luchthoeveelheid voor ventilatiestand 3 in.", "50..300 (Medium) / 50..400 (Large)", "225 Medium / 300 Large"},
    {"U4", "Minimum buitentemperatuur bypass", "Dit is de minimum buitenluchttemperatuur waarbij de bypass zich opent, wanneer ook de binnenluchttemperatuur aan de voorwaarden voldoet.", "5..20", "10"},
    {"U5", "Minimum binnentemperatuur bypass", "Dit is de minimum binnenluchttemperatuur waarbij de bypass zich opent, wanneer ook de buitenluchttemperatuur aan de voorwaarden voldoet.", "18..30", "22"},
    {"U8", "Niet van toepassing", "Niet van toepassing.", "0,1", "0 (uit)"},
    {"I1", "Vaste onbalans", "Hiermee kan de woning op overdruk (+) dan wel onderdruk (-) worden gezet.", "-100..+100", "0"},
    {"I2", "Geen contact stap", "Deze instelling bepaalt de ventilatiestand wanneer geen schakelcontact is aangesloten op stand 1; het toestel gaat hier op de ingestelde ventilatiestand draaien.", "0,1,2,3", "1"},
    {"I3", "Perilex L2 stap", "Bepaalt de ventilatiestand wanneer L2 van de perilexkabel spanning krijgt. Er kan worden gekozen tussen stand 2 en stand 3.", "2,3", "2"},
    {"I4", "Switch lijn 1 stap", "Bepaalt welke stand van de standenschakelaar overeenkomt met lijn1 op de besturingsunit.", "0,1,2,3", "1"},
    {"I5", "Switch lijn 2 stap", "Bepaalt welke stand van de standenschakelaar overeenkomt met lijn2 op de besturingsunit.", "0,1,2,3", "2"},
    {"I6", "Switch lijn 3 stap", "Bepaalt welke stand van de standenschakelaar overeenkomt met lijn3 op de besturingsunit.", "0,1,2,3", "3"},
    {"I7", "Onbalans toelaatbaar", "Hiermee wordt bepaald of bijvoorbeeld de vorstregeling mag ingrijpen op de balans.", "0,1", "1 (ja)"},
    {"I8", "Bypass-modus", "Bepaalt of de bypassklep nooit schakelt, automatisch opent of de toevoerventilator bij bypass zo laag mogelijk toerental draait.", "0,1,2", "1"},
    {"I9", "Hysterese bypass", "Hiermee kan worden opgegeven hoeveel de binnentemperatuur mag worden verlaagd alvorens de bypass sluit of de toevoerventilator het normale toerental gaat draaien.", "0..5", "2"},
    {"I10", "Constante druk uitgeschakeld", "Hiermee kan worden bepaald of de ventilatoren in alle gevallen constant flow draaien of bij overschrijden van bepaalde weerstand constant druk gaan draaien.", "0,1", "0 (nee)"},
    {"I11", "Voor- of naverwarmer", "Hiermee wordt opgegeven of een voor- of naverwarmer aangesloten is.", "0,1,2,3", "0"},
    {"I12", "Offset temperatuur voorverwarmer", "Hiermee wordt offset temperatuur voorverwarmer ingesteld.", "-30..+30", "0,5"},
    {"I13", "Filtermelding aan/uit", "Bepaalt of de filtermelding getoond wordt op display en het led van de 3-standenschakelaar.", "1,0", "1 (aan)"},
    {"I14", "Optieprint aanwezig", "Hiermee wordt aangegeven of een optieprint is gemonteerd.", "1,0", "0 (nee)"},
    {"I15", "WTW configuratie", "Keuze instelling wanneer een WTW samen met CV wordt gebruikt; alleen WTW of de combinatie CV + WTW.", "0,1", "0 (WTW)"},
    {"I16", "Ventilator uit", "Ventilator(en) uit bij CV + WTW (alleen van toepassing indien I15 = 1).", "1,2,3", "1 (afvoerventilator)"},
    {"I17", "Repeatertijd", "Repeatertijd in uren van het uitschakelen van de onder I16 geselecteerde ventilator(en) bij CV + WTW.", "1..24", "24 (uur)"},
    {"I18", "Minimale uitschakeltijd ventilator(en)", "Maximale uitschakeltijd in seconden van de onder I16 geselecteerde ventilator(en) bij CV + WTW.", "1..240", "60 (seconden)"},
    {"I19", "Minimale uitschakeltijd ventilator(en) na inschakelen 230V", "Minimale uitschakeltijd in seconden van de onder I16 geselecteerde ventilator(en) na inschakelen 230V. bij CV + WTW.", "1..240", "1 (seconde)"},
};
}  // namespace

size_t getParameterDefinitionCount() {
  return sizeof(kParameterDefinitions) / sizeof(kParameterDefinitions[0]);
}

const ParameterDefinition *getParameterDefinitionAt(size_t index) {
  if (index >= getParameterDefinitionCount()) {
    return nullptr;
  }

  return &kParameterDefinitions[index];
}
