/*
 mind.cpp     MindForger thinking notebook

 Copyright (C) 2016-2018 Martin Dvorak <martin.dvorak@mindforger.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "mind.h"

using namespace std;

namespace m8r {

Mind::Mind(Configuration &configuration)
    : config{configuration},
      memory{configuration},
      exclusiveMind{}
{
    ai = new Ai{memory,*this};
    activeProcesses = 0;
    memory.setTimeScope(&timeScopeAspect);

    this->mdConfigRepresentation
        = new MarkdownConfigurationRepresentation{};
}

Mind::~Mind()
{
    delete ai;
    delete mdConfigRepresentation;

    // - Memory destruct outlines
    // - allNotesCache Notes is just container referencing Memory's Outlines
}

/*
 * THINKING
 */

bool Mind::learn()
{
    MF_DEBUG("@Learn" << endl);
    lock_guard<mutex> criticalSection{exclusiveMind};

    if(config.getMindState()!=Configuration::MindState::DREAMING && !activeProcesses) {
        MF_DEBUG("Learning..." << endl);
        mindAmnesia();
        memory.learn();
        MF_DEBUG("Mind LEARNED" << endl);
        return true;
    } else {
        MF_DEBUG("Learn: CANNOT learn because Mind is DREAMING" << endl);
        return false;
    }
}

future<bool> Mind::think()
{
    MF_DEBUG("@Think" << endl);
    lock_guard<mutex> criticalSection{exclusiveMind};

    if(config.getMindState()==Configuration::MindState::SLEEPING) {
        // get ready for thinking - dream() changes state to THINKING on its finish
        return mindDream();
    } else {
        MF_DEBUG("Think: CANNOT think because Mind is DREAMING or already THINKING (asleep first)" << endl);
        promise<bool> p;
        p.set_value(false);
        return p.get_future();
    }
}

/* It does NOT need mutex because it's private and can be called from Mind only.
 * This method may run long time. It ALWAYS switches mind state to THINKING when finishes.
 */
future<bool> Mind::mindDream()
{
    MF_DEBUG("@Dream" << endl);

    // Mind is expected to be clean and SLEEPING
    if(config.getMindState()==Configuration::MindState::SLEEPING) {
        MF_DEBUG("Dreaming..." << endl);
        config.setMindState(Configuration::MindState::DREAMING);

        // sanity
        // o integrity check: ...
        // o memory structure check:
        //  - Os w/o description
        //  - Os w/o any N
        //  - Ns w/o description
        // o attachments
        //  - orphan attachments (not referenced from any O)

        // triples: infer all triples, check, fix, optimize and save

        // AI: AA, NN, ... may take long time to finish
        return ai->dream();
    } else {
        MF_DEBUG("Dream: CANNOT dream because Mind is not ready ~ SLEEPING (asleep first)" << endl);
        promise<bool> p;
        p.set_value(false);
        return p.get_future();
    }
}

bool Mind::sleep()
{
    MF_DEBUG("@Sleep" << endl);
    lock_guard<mutex> criticalSection{exclusiveMind};
    return mindSleep();
}

/*
 *  This method does NOT need mutex because it's private and it's called from Mind only
 */
bool Mind::mindSleep()
{
    if(config.getMindState()!=Configuration::MindState::DREAMING && !activeProcesses) {
        // AI can asleep ONLY if there are no active mental processes
        if(ai->sleep()) {
            allNotesCache.clear();
            memoryDwell.clear();
            triples.clear();

            config.setMindState(Configuration::MindState::SLEEPING);
            mdConfigRepresentation->save(config);
            MF_DEBUG("Mind IS sleeping..." << endl);
            return true;
        } else {
            // AI cannot asleep because there are active mind processes that use Mind/AI data
            MF_DEBUG("Sleep: CANNOT asleep because there are active Mind processes" << endl);
            return false;
        }
    } else {
        MF_DEBUG("Sleep: CANNOT asleep because Mind is DREAMING (wait for dreaming to finish)" << endl);
        // DREAMING cannot be cancelled > wait for dream() to finish before calling sleep() again.
        return false;
    }
}

bool Mind::amnesia()
{
    MF_DEBUG("@Amnesia" << endl);
    lock_guard<mutex> criticalSection{exclusiveMind};
    return mindAmnesia();
}

future<vector<pair<Note*,float>>> Mind::getAssociationsLeaderboard(const Note* n) {
    return ai->getAssociationsLeaderboard(n);
}

/*
 *  This method does NOT need mutex because it's private and it's called from Mind only
 */
bool Mind::mindAmnesia()
{
    if(config.getMindState()!=Configuration::MindState::DREAMING && !activeProcesses) {
        mindSleep();

        // forget EVERYTHING
        memory.amnesia();

        MF_DEBUG("Mind WITH amnesia" << endl);
        return true;
    } else {
        MF_DEBUG("Amnesia: CANNOT asleep because Mind is DREAMING (waith for dreaming to finish)" << endl);
        return false;
    }
}

/*
 * REMEMBERING
 */

const vector<Note*>& Mind::getMemoryDwell(int pageSize) const
{
    UNUSED_ARG(pageSize);

    return memoryDwell;
}

size_t Mind::getMemoryDwellDepth() const
{
    return memoryDwell.size();
}

vector<Note*>* Mind::findNoteByNameFts(const string& regexp) const
{
    UNUSED_ARG(regexp);

    return nullptr;
}

void Mind::getOutlineNames(vector<string>& names) const
{
    // IMPROVE PERF cache vector (stack member) until and evict on memory modification
    vector<Outline*> outlines = memory.getOutlines();
    for(Outline* outline:outlines) {
        names.push_back(outline->getName());
    }
}

void Mind::findNoteFts(vector<Note*>* result, const string& regexp, const bool ignoreCase, Outline* outline)
{
    string s{};

    // IMPROVE make this faster - do NOT convert to lower case, but compare it in that method > will do less
    if(ignoreCase) {
        // case INSENSITIVE
        s.clear();
        stringToLower(outline->getName(), s);
        if(s.find(regexp)!=string::npos) {
            result->push_back(outline->getOutlineDescriptorAsNote());
        } else {
            for(string* d:outline->getDescription()) {
                if(d) {
                    s.clear();
                    stringToLower(*d, s);
                    if(s.find(regexp)!=string::npos) {
                        result->push_back(outline->getOutlineDescriptorAsNote());
                        break;
                    }
                }
            }
        }
        for(Note* note:outline->getNotes()) {
            if(timeScopeAspect.isEnabled() && timeScopeAspect.isOutOfScope(note)) {
                continue;
            }
            s.clear();
            stringToLower(note->getName(), s);
            if(s.find(regexp)!=string::npos) {
                result->push_back(note);
            } else {
                for(string* d:note->getDescription()) {
                    if(d) {
                        s.clear();
                        stringToLower(*d, s);
                        if(s.find(regexp)!=string::npos) {
                            result->push_back(note);
                            break;
                        }
                    }
                }
            }
        }
    } else {
        // case SENSITIVE
        if(outline->getName().find(regexp)!=string::npos) {
            result->push_back(outline->getOutlineDescriptorAsNote());
        } else {
            for(string* d:outline->getDescription()) {
                if(d && d->find(regexp)!=string::npos) {
                    result->push_back(outline->getOutlineDescriptorAsNote());
                    // avoid multiple matches in the result
                    break;
                }
            }
        }
        for(Note* note:outline->getNotes()) {
            if(note->getName().find(regexp)!=string::npos) {
                result->push_back(note);
            } else {
                for(string* d:note->getDescription()) {
                    if(d && d->find(regexp)!=string::npos) {
                        result->push_back(note);
                        // avoid multiple matches in the result
                        break;
                    }
                }
            }
        }
    }
}

vector<Note*>* Mind::findNoteFts(const string& regexp, const bool ignoreCase, Outline* scope)
{
    if(allNotesCache.size()) {
        allNotesCache.clear();
    }

    vector<Note*>* result = new vector<Note*>();

    string r{};
    if(ignoreCase) {
        stringToLower(regexp, r);
    } else {
        r.append(regexp);
    }

    if(scope) {
        findNoteFts(result, r, ignoreCase, scope);
    } else {
        const vector<m8r::Outline*> outlines = memory.getOutlines();
        for(Outline* outline:outlines) {
            findNoteFts(result, r, ignoreCase, outline);
        }
    }
    return result;
}

vector<Note*>* Mind::getReferencedNotes(const Note& note) const
{
    UNUSED_ARG(note);

    return nullptr;
}

vector<Note*>* Mind::getReferencedNotes(const Note& note, const Outline& outline) const
{
    UNUSED_ARG(note);
    UNUSED_ARG(outline);

    return nullptr;
}

vector<Note*>* Mind::getRefereeNotes(const Note& note) const
{
    UNUSED_ARG(note);

    return nullptr;
}

vector<Note*>* Mind::getRefereeNotes(const Note& note, const Outline& outline) const
{
    UNUSED_ARG(note);
    UNUSED_ARG(outline);

    return nullptr;
}

vector<Note*>* Mind::getTaggedNotes(const vector<Tag*> tags) const
{
    UNUSED_ARG(tags);

    return nullptr;
}

// IMPROVE PERF use dirty flag to avoid result-rebuilt
const vector<Outline*>& Mind::getOutlines() const
{
    static vector<Outline*> result{};

    if(timeScopeAspect.isEnabled()) {
        result.clear();
        for(Outline* o:memory.getOutlines()) {
            if(timeScopeAspect.isInScope(o)) {
                result.push_back(o);
            }
        }
        return result;
    } else {
        return memory.getOutlines();
    }
}

vector<Outline*>* Mind::getOutlinesOfType(const OutlineType& type) const
{
    UNUSED_ARG(type);

    return nullptr;
}

void Mind::getAllNotes(std::vector<Note*>& notes) const
{
    return memory.getAllNotes(notes);
}

vector<Note*>* Mind::getNotesOfType(const NoteType& type) const
{
    UNUSED_ARG(type);

    return nullptr;
}

vector<Note*>* Mind::getNotesOfType(const NoteType& type, const Outline& outline) const
{
    UNUSED_ARG(type);
    UNUSED_ARG(outline);

    return nullptr;
}

vector<Note*>* Mind::getAssociatedNotes(const Note& note) const
{
    UNUSED_ARG(note);

    return nullptr;
}

vector<Note*>* Mind::getAssociatedNotes(const Note& note,
        const Outline& outline) const
{
    UNUSED_ARG(note);
    UNUSED_ARG(outline);

    return nullptr;
}

vector<Note*>* Mind::getAssociatedNotes(const vector<string*> words) const
{
    UNUSED_ARG(words);

    return nullptr;
}

vector<Note*>* Mind::getAssociatedNotes(const vector<string*> words,
        const Outline& outline) const
{
    UNUSED_ARG(words);
    UNUSED_ARG(outline);

    return nullptr;
}

vector<Outline*>* Mind::getTaggedOutlines(
        const std::vector<Tag*> labels) const
{
    UNUSED_ARG(labels);

    return nullptr;
}

vector<Tag*>* Mind::getOutlinesTags() const
{
    return nullptr;
}

vector<Tag*>* Mind::getTags() const
{
    return nullptr;
}

vector<Tag*>* Mind::getNoteTags(const Outline& outline) const
{
    UNUSED_ARG(outline);

    return nullptr;
}

unsigned Mind::getTagCardinality(const Tag& tag) const
{
    UNUSED_ARG(tag);

    return 0;
}

unsigned Mind::getOutlineTagCardinality(const Tag& tag) const
{
    UNUSED_ARG(tag);

    return 0;
}

unsigned Mind::getNoteTagCardinality(const Tag& tag) const
{
    UNUSED_ARG(tag);

    return 0;
}

string Mind::outlineNew(
    // IMPROVE pass name by reference
    const string* name,
    const OutlineType* outlineType,
    const int8_t importance,
    const int8_t urgency,
    const int8_t progress,
    const vector<const Tag*>* tags,
    const vector<string*>* preamble,
    Stencil* outlineStencil)
{
    string key = memory.createOutlineKey(name);
    Outline* outline{};
    if(outlineStencil) {
        outline = memory.createOutline(outlineStencil);
        outline->setModified();
    } else {
        outline = new Outline{ontology().getDefaultOutlineType()};
    }

    if(preamble && preamble->size()) {
        outline->setPreamble(*preamble);
    }

    if(outline) {
        outline->completeProperties(datetimeNow());
        outline->setKey(key);
        if(name && !name->empty()) {
            outline->setName(*name);
        }
        if(outlineType) {
            outline->setType(outlineType);
        }
        outline->setImportance(importance);
        outline->setUrgency(urgency);
        outline->setProgress(progress);
        if(tags) {
            for(const Tag* t:*tags) {
                outline->addTag(t);
            }
        }
        if(outline->getNotes().empty()) {
            Note* note = new Note{memory.getOntology().getNoteTypes().get(NoteType::KeyNote()), outline};
            note->completeProperties(datetimeNow());
            outline->addNote(note);
        }


        memory.remember(outline);
        onRemembering();
    } else {
        throw MindForgerException("Unable to create new Outline!");
    }

    return outline?outline->getKey():nullptr;
}

Outline* Mind::outlineClone(const std::string& outlineKey)
{
    Outline* o = memory.getOutline(outlineKey);
    if(o) {
        Outline* clonedOutline = new Outline{*o};
        clonedOutline->setKey(memory.createOutlineKey(&o->getName()));
        memory.remember(clonedOutline);
        onRemembering();
        return clonedOutline;
    } else {
        return nullptr;
    }
}

bool Mind::outlineForget(string outlineKey)
{
    Outline* o = memory.getOutline(outlineKey);
    if(o) {
        memory.forget(o);
        auto k = memory.createLimboKey(&o->getName());
        o->setKey(k);
        moveFile(outlineKey, k);
        return true;
    }
    return false;
}

Note* Mind::noteNew(
        const std::string& outlineKey,
        const uint16_t offset,
        // IMPROVE pass name by reference
        const std::string* name,
        const NoteType* noteType,
        u_int16_t depth,
        const std::vector<const Tag*>* tags,
        const int8_t progress,
        Stencil* noteStencil)
{
    Outline* o = memory.getOutline(outlineKey);
    if(o) {
        Note* n = memory.createNote(noteStencil);
        if(!n) {
            // IMPROVE make note type method parameter w/ a default
            n = new Note(ontology().findOrCreateNoteType(NoteType::KeyNote()),o);
        }
        n->setOutline(o);
        if(name) {
            n->setName(*name);
        }
        n->setModified();
        n->setModifiedPretty();
        if(noteType) {
            n->setType(noteType);
        }
        n->setDepth(depth);
        if(tags) {
            n->setTags(tags);
        }
        n->setProgress(progress);
        n->completeProperties(n->getModified());

        o->addNote(n, NO_PARENT==offset?0:offset);
        return n;
    } else {
        throw MindForgerException("Outline for given key not found!");
    }
}

Note* Mind::noteClone(const string& outlineKey, const Note* newNote)
{
    Outline* o = memory.getOutline(outlineKey);
    if(o) {
        return o->cloneNote(newNote);
    } else {
        throw MindForgerException("Outline for given key not found!");
    }
}

Outline* Mind::noteRefactor(Note* noteToRefactor, const string& targetOutlineKey, Note* targetParent)
{
    UNUSED_ARG(targetParent);

    if(noteToRefactor) {
        Outline* targetOutline = memory.getOutline(targetOutlineKey);
        if(targetOutline) {
            vector<Note*> children{};
            Outline* sourceOutline = noteToRefactor->getOutline();
            sourceOutline->getNoteChildren(noteToRefactor, &children);
            children.insert(children.begin(), noteToRefactor);
            // IMPROVE allow passing parent for the Note in the target Outline
            targetOutline->addNotes(children, 0);

            sourceOutline->removeNote(noteToRefactor);

            memory.remember(sourceOutline);
            memory.remember(targetOutline);

            return targetOutline;
        } else {
            throw MindForgerException("Outline for given key not found!");
        }
    } else {
        throw MindForgerException("Note to be refactored is nullptr!");
    }
}

Outline* Mind::noteForget(Note* note)
{
    Outline* o = note->getOutline();
    if(o) {
        note->getOutline()->forgetNote(note);
        return o;
    } else {
        throw MindForgerException("Unable find Outline from which should be the Note deleted!");
    }
}

void Mind::noteUp(Note* note, Outline::Patch* patch)
{
    if(note) {
        note->getOutline()->moveNoteUp(note, patch);
    }
}

void Mind::noteDown(Note* note, Outline::Patch* patch)
{
    if(note) {
        note->getOutline()->moveNoteDown(note, patch);
    }
}

void Mind::noteFirst(Note* note, Outline::Patch* patch)
{
    if(note) {
        note->getOutline()->moveNoteToFirst(note, patch);
    }
}

void Mind::noteLast(Note* note, Outline::Patch* patch)
{
    if(note) {
        note->getOutline()->moveNoteToLast(note, patch);
    }
}

void Mind::notePromote(Note* note, Outline::Patch* patch)
{
    if(note) {
        note->getOutline()->promoteNote(note, patch);
    }
}

void Mind::noteDemote(Note* note, Outline::Patch* patch)
{
    if(note) {
        note->getOutline()->demoteNote(note, patch);
    }
}

void Mind::onRemembering()
{
    allNotesCache.clear();
}

// unique_ptr template BREAKS Qt Developer indentation > stored at EOF
unique_ptr<vector<Outline*>> Mind::findOutlineByNameFts(const string& expr) const
{
    // IMPROVE implement regexp and other search options by reusing HSTR code
    // IMPROVE PERF this method is extremely inefficient > use cached map (stack member) evicted on memory modification
    unique_ptr<vector<Outline*>> result{new vector<Outline*>()};
    if(expr.size()) {
        vector<Outline*> outlines = memory.getOutlines();
        for(Outline* outline:outlines) {
            if(!expr.compare(outline->getName())) {
                result->push_back(outline);
            }
        }
    }
    return result;
}

} /* namespace */
