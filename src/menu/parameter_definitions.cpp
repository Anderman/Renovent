#include "menu/parameter_definitions.h"

namespace {
constexpr ParameterDefinition kParameterDefinitions[] = {
    {"U1", "Volume stap 1", "Stelt de luchthoeveelheid voor ventilatiestand 1 in.", "50..(max-10)", "100"},
    {"U2", "Volume stap 2", "Stelt de luchthoeveelheid voor ventilatiestand 2 in.", "50..(max-5)", "Medium 150 / Large 200"},
    {"U3", "Volume stap 3", "Stelt de luchthoeveelheid voor ventilatiestand 3 in.", "50..300 (Medium) / 50..400 (Large)", "225 Medium / 300 Large"},
    {"U4", "Minimum buitentemperatuur bypass", "Dit is de minimum buitenluchttemperatuur waarbij de bypass zich opent, wanneer ook de binnenluchttemperatuur aan de voorwaarden voldoet.", "5..20", "10"},
    {"U5", "Minimum binnentemperatuur bypass", "Dit is de minimum binnenluchttemperatuur waarbij de bypass zich opent, wanneer ook de buitenluchttemperatuur aan de voorwaarden voldoet.", "18..30", "22"},
    {"U6", "Streeftemperatuur naverwarmer", "Streeftemperatuur naverwarmer, alleen van toepassing indien naverwarmer is gemonteerd.", "0..30", "0"},
    {"U7", "Modus proportionele ingangen", "Bepaalt hoe de regeling reageert op de voltages bij de proportionele ingangen: A = alleen 3-standenschakelaar, B = proportionele ingang 1, C = proportionele ingang 2, D = proportionele ingang 1 en 2 waarbij ingang 1 voorrang heeft.", "A,B,C,D", "A"},
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
    {"P1", "Toevoervolume bij calamiteit", "Luchthoeveelheid toevoerventilator wanneer de calamiteiteningang wordt geschakeld.", "0..max", "0"},
    {"P2", "Afvoervolume bij calamiteit", "Luchthoeveelheid afvoerventilator wanneer de calamiteiteningang wordt geschakeld.", "0..max", "0"},
    {"P3", "Slaapkamercorrectie toevoer", "Extra luchthoeveelheid toevoerventilator wanneer ingang slaapkamerklep wordt gesloten.", "-100..+100", "-20"},
    {"P4", "Slaapkamercorrectie afvoer", "Extra luchthoeveelheid afvoerventilator wanneer ingang slaapkamerklep wordt gesloten.", "-100..+100", "-20"},
    {"P5", "Koppeling maakcontact 1", "Bepaalt hoe programmeerbaar maakcontact 1 wordt gekoppeld aan andere functies.", "0,1,2,3,4", "0"},
    {"P6", "Toevoermodus maakcontact 1", "Bepaalt hoe de toevoerventilator reageert als maakcontact 1 wordt gemaakt.", "0,1,2,3", "0"},
    {"P7", "Afvoermodus maakcontact 1", "Bepaalt hoe de afvoerventilator reageert als maakcontact 1 wordt gemaakt.", "0,1,2,3", "1"},
    {"P8", "Koppeling maakcontact 2", "Bepaalt hoe programmeerbaar maakcontact 2 wordt gekoppeld aan andere functies.", "0,1,2,3,4", "0"},
    {"P9", "Toevoermodus maakcontact 2", "Bepaalt hoe de toevoerventilator reageert als maakcontact 2 wordt gemaakt.", "0,1,2,3", "0"},
    {"P10", "Afvoermodus maakcontact 2", "Bepaalt hoe de afvoerventilator reageert als maakcontact 2 wordt gemaakt.", "0,1,2,3", "1"},
    {"P11", "Streefspanning proportioneel 1", "Bepaalt het streefvoltage van de proportionele ingang 1. De volumeregeling probeert binnen de gestelde voorwaarden het ingangsvoltage naar het streefvoltage te regelen.", "0..10", "8"},
    {"P12", "Maximumspanning proportioneel 1", "Bepaalt het maximale voltage van het op de proportionele ingang 1 aangesloten apparaat. De proportionele band van de PI-regelaar wordt automatisch aangepast.", "0..10", "10"},
    {"P13", "Integratietijd proportioneel 1", "Bepaalt de integratietijd van de PI-regelaar van de proportionele ingang 1. De PI-regelaar regelt zuiver proportioneel als de integratietijd 0 seconden is.", "0..1250", "0"},
    {"P14", "Streefspanning proportioneel 2", "Bepaalt het streefvoltage van de proportionele ingang 2. De volumeregeling probeert binnen de gestelde voorwaarden het ingangsvoltage naar het streefvoltage te regelen.", "0..10", "4"},
    {"P15", "Maximumspanning proportioneel 2", "Bepaalt het maximale voltage van het op de proportionele ingang 2 aangesloten apparaat. De proportionele band van de PI-regelaar wordt automatisch aangepast.", "0..10", "10"},
    {"P16", "Integratietijd proportioneel 2", "Bepaalt de integratietijd van de PI-regelaar van de proportionele ingang 2. De PI-regelaar regelt zuiver proportioneel als de integratietijd 0 seconden is.", "0..1250", "0"},
    {"P17", "Voorverwarmer aan-afwezig", "Bepaalt of de voorverwarmer aan- of afwezig is. Wanneer de voorverwarmer wordt aangestuurd maar niet aanwezig is, zal bij vorst de toevoerventilator naar het minimum gaan.", "0,1", "0 (nee)"},
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
