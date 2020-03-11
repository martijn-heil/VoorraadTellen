# VoorraadTellen
Een simpel maar effectief programma om voorraad (bijv: producten in een winkel) 
sneller te tellen dan als het ouderwets handmatig of in Microsoft Excel zou kunnen.

Dit programma bestaat uit een interactieve 
[Command Line Interface](https://nl.wikipedia.org/wiki/Command-line-interface)
waarin de workflow van het tellen van voorraad maximaal is geoptimaliseerd.
Het is primair bedoeld voor gebruik op een laptop of soortgelijk apparaat, die in
verbinding staat met een barcode-scanner (bijv. d.m.v. USB) die de gescande 
barcode wegschrijft naar stdin of waar de cursor momenteel staat.

Het programma is zeer makkelijk te gebruiken.

De interface lijkt misschien wat oudbollig, maar zo kan dit programma draaien
op elke denkbare computer en elk denkbaar [besturingssysteem](https://nl.wikipedia.org/wiki/Besturingssysteem).

Echter, voor meer ingewikkelde situaties is dit programma minder geschikt.
Zo kun je eigenlijk niet met meerdere mensen tegelijk een voorraad tellen, omdat dit programma
rechtstreeks opereert op een [CSV-bestand](https://nl.wikipedia.org/wiki/Kommagescheiden_bestand) van bekende producten.

Het programma is geschreven in de programmeertaal C, volgens de standaard [C11](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
Het zou met relatief weinig moeite geport kunnen worden naar oudere C standaarden.
Voor Windows kan dit programma gecompileerd worden met [MinGW](http://www.mingw.org/).
Op Linux en andere *nix systemen zou elke standards-compliant compiler 
moeten werken, maar [GCC](https://gcc.gnu.org/) heeft de voorkeur.

Dit programma is vrijgegeven onder de *GNU General Public License v3.0*, 
zie de [licentie](LICENSE) voor de details, of zie [de website van GNU](https://www.gnu.org/licenses/gpl-3.0.nl.html)
voor samenvattingen en uitleg.