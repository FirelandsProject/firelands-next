#include <infrastructure/network/sessions/WorldSession.h>
#include <cstdlib>

namespace Firelands {

WorldSession::WorldSession(
    tcp::socket socket, std::shared_ptr<AuthService> authService,
    std::shared_ptr<CharacterService> charService,
    std::shared_ptr<ICommandService> commandService,
    std::shared_ptr<MySqlAccountDataRepository> accountDataRepo,
    std::shared_ptr<LanguagesDbc const> languagesDbc,
    std::shared_ptr<ISpellDefinitionStore const> spellDefinitions,
    std::shared_ptr<IRealmRepository> realmRepo,
    std::shared_ptr<OnlineCharacterSessionRegistry> onlineCharRegistry,
    std::shared_ptr<GmTicketService> gmTicketService,
    std::shared_ptr<ItemDbHotfixStore const> itemDbHotfix,
    std::shared_ptr<SpellManager> spellManager,
    std::shared_ptr<INpcTemplateSearchRepository const> npcTemplateSearch,
    std::shared_ptr<FactionTemplateDbc const> factionTemplateDbc,
    std::shared_ptr<IGossipRepository> gossipRepo,
    std::shared_ptr<INpcTextRepository> npcTextRepo)
    : _socket(std::move(socket)), _authService(std::move(authService)),
      _charService(std::move(charService)),
      _commandService(std::move(commandService)),
      _accountDataRepo(std::move(accountDataRepo)),
      _languagesDbc(std::move(languagesDbc)),
      _spellDefinitions(std::move(spellDefinitions)),
      _realmRepo(std::move(realmRepo)),
      _onlineCharRegistry(std::move(onlineCharRegistry)),
      _gmTicketService(std::move(gmTicketService)),
      _itemDbHotfix(std::move(itemDbHotfix)),
      _spellManager(std::move(spellManager)),
      _npcTemplateSearch(std::move(npcTemplateSearch)),
      _factionTemplateDbc(std::move(factionTemplateDbc)),
      _gossipRepo(std::move(gossipRepo)),
      _npcTextRepo(std::move(npcTextRepo)), _serverSeed(0),
      _accountId(0), _timeSyncPeriodicTimer(_socket.get_executor()),
      _pendingSpellCastTimer(_socket.get_executor()) {}

WorldSession::~WorldSession() {
  if (_playerGuid != 0) {
    FinalizeWorldExit();
  } else {
    UnregisterFromOnlineCharacterRegistryIfNeeded();
  }
}

} // namespace Firelands
